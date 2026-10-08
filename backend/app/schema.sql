CREATE TABLE IF NOT EXISTS readings (
    device_id          text        NOT NULL,
    ts                 timestamptz NOT NULL,
    received_at        timestamptz NOT NULL DEFAULT now(),
    scan_window_s      smallint    NOT NULL,
    rssi_min           smallint    NOT NULL,
    devices            integer     NOT NULL,
    devices_above_rssi integer     NOT NULL,
    avg_rssi           smallint    NOT NULL,
    temp_c             real,
    humidity_pct       real,
    lux                real,
    PRIMARY KEY (device_id, ts)
);

CREATE INDEX IF NOT EXISTS readings_ts_idx ON readings (ts);
