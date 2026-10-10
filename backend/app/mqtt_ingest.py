import logging
import sys
from datetime import datetime, timezone

import paho.mqtt.client as mqtt

from app.db import PostgresRepository
from app.handlers import IngestOutcome, ingest_message
from app.repository import ReadingRepository
from app.settings import TLS_PORT, MqttSettings, get_mqtt_settings

CLIENT_ID = "guzvomator-ingest"
QOS = 1

log = logging.getLogger("guzvomator.mqtt")


def build_client(
    settings: MqttSettings, repository: ReadingRepository
) -> mqtt.Client:
    """A message is acknowledged only after it was handled, so a storage failure
    leaves it with the broker, which sends it again to the restarted service."""
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id=CLIENT_ID,
        clean_session=False,
        manual_ack=True,
    )
    if settings.user:
        client.username_pw_set(settings.user, settings.password)
    if settings.port == TLS_PORT:
        client.tls_set()

    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            log.error("broker refused the connection: %s", reason_code)
            return
        client.subscribe(settings.topic, qos=QOS)
        log.info("subscribed to %s", settings.topic)

    def on_message(client, userdata, message):
        try:
            outcome = ingest_message(
                repository, message.payload, datetime.now(timezone.utc)
            )
        except Exception:
            log.exception("storing a reading failed, leaving it unacknowledged")
            client.disconnect()
            return
        if outcome is IngestOutcome.INVALID:
            log.warning("dropped an invalid reading on %s", message.topic)
        client.ack(message.mid, message.qos)

    client.on_connect = on_connect
    client.on_message = on_message
    return client


def main() -> int:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s %(message)s")
    settings = get_mqtt_settings()
    client = build_client(settings, PostgresRepository(settings.database_url))
    client.connect_async(settings.host, settings.port)
    client.loop_forever()
    return 1


if __name__ == "__main__":
    sys.exit(main())
