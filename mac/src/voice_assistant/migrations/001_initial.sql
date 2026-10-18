-- Desk-Mate voice assistant migration 001
-- Privacy: no tokens, keys, full web pages or screenshots are stored here.
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS schema_migrations (
  version INTEGER PRIMARY KEY,
  applied_at REAL NOT NULL
);

CREATE TABLE IF NOT EXISTS devices (
  id TEXT PRIMARY KEY,
  token_hash TEXT NOT NULL UNIQUE,
  created_at REAL NOT NULL,
  revoked_at REAL,
  display_name TEXT
);

CREATE TABLE IF NOT EXISTS commands (
  id TEXT PRIMARY KEY,
  device_id TEXT REFERENCES devices(id),
  boot_id TEXT,
  kind TEXT NOT NULL CHECK (kind IN ('audio','replay')),
  source TEXT NOT NULL CHECK (source IN ('esp32_ptt','esp32_replay','mac_manual','mac_wakeword')),
  state TEXT NOT NULL CHECK (state IN (
    'STAGING','SEALED','COMMITTED','TRANSCRIBING','PLANNING',
    'AWAITING_CONFIRMATION','EXECUTING','SUCCEEDED','PARTIAL','FAILED',
    'CANCELED','ABANDONED','INTERRUPTED','OUTCOME_UNKNOWN')),
  error_code TEXT,
  created_at REAL NOT NULL,
  updated_at REAL NOT NULL,
  committed_at REAL,
  expires_at REAL,
  pcm_bytes INTEGER,
  frame_count INTEGER,
  pcm_sha256 TEXT,
  payload_sha256 TEXT,
  request_fingerprint TEXT,
  raw_text TEXT,
  normalized_text TEXT,
  plan_json TEXT,
  model_digest TEXT,
  prompt_version TEXT,
  schema_version INTEGER,
  total_latency_ms INTEGER,
  replay_of TEXT REFERENCES commands(id),
  staging_path TEXT,
  wav_path TEXT,
  summary TEXT,
  result_json TEXT
);
CREATE INDEX IF NOT EXISTS idx_commands_device_created ON commands(device_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_commands_state ON commands(state);
CREATE INDEX IF NOT EXISTS idx_commands_replay_of ON commands(replay_of);

CREATE TABLE IF NOT EXISTS action_steps (
  command_id TEXT NOT NULL REFERENCES commands(id) ON DELETE CASCADE,
  step_index INTEGER NOT NULL,
  action TEXT NOT NULL,
  resolved_params_json TEXT,
  state TEXT NOT NULL CHECK (state IN (
    'PENDING','PREPARED','STARTED','SUCCEEDED','NOOP_VERIFIED',
    'FAILED','SKIPPED','OUTCOME_UNKNOWN')),
  started_at REAL,
  completed_at REAL,
  output_json TEXT,
  evidence_json TEXT,
  error_code TEXT,
  PRIMARY KEY (command_id, step_index)
);

CREATE TABLE IF NOT EXISTS audio_samples (
  command_id TEXT PRIMARY KEY REFERENCES commands(id) ON DELETE CASCADE,
  relative_path TEXT NOT NULL UNIQUE,
  hash TEXT NOT NULL,
  size_bytes INTEGER NOT NULL,
  sample_rate INTEGER NOT NULL CHECK (sample_rate = 16000),
  channels INTEGER NOT NULL CHECK (channels = 1),
  bit_depth INTEGER NOT NULL CHECK (bit_depth = 16),
  duration_ms INTEGER NOT NULL,
  rms REAL,
  peak REAL,
  clipped_ratio REAL,
  quality_label TEXT NOT NULL CHECK (quality_label IN ('good','noisy','clipped','silent','unknown')),
  retention_until REAL,
  pinned INTEGER NOT NULL CHECK (pinned IN (0,1)) DEFAULT 0,
  deletion_state TEXT NOT NULL CHECK (deletion_state IN ('active','deleting','deleted')) DEFAULT 'active'
);

CREATE TABLE IF NOT EXISTS feedback (
  id TEXT PRIMARY KEY,
  command_id TEXT NOT NULL REFERENCES commands(id) ON DELETE CASCADE,
  kind TEXT NOT NULL CHECK (kind IN ('correction','explicit_success','redo','cancel_after')),
  corrected_text TEXT,
  approved_plan_json TEXT,
  human_reviewed INTEGER NOT NULL CHECK (human_reviewed IN (0,1)) DEFAULT 0,
  created_at REAL NOT NULL
);

CREATE TABLE IF NOT EXISTS preferences (
  key TEXT PRIMARY KEY,
  value_json TEXT NOT NULL,
  source TEXT NOT NULL CHECK (source IN ('manual','suggestion_accepted')),
  updated_at REAL NOT NULL
);

CREATE TABLE IF NOT EXISTS model_versions (
  id TEXT PRIMARY KEY,
  model_digest TEXT UNIQUE NOT NULL,
  dataset_hash TEXT,
  manifest_json TEXT NOT NULL,
  lifecycle TEXT NOT NULL CHECK (lifecycle IN ('candidate','shadow','approved','retired')),
  activated_at REAL
);

CREATE TABLE IF NOT EXISTS training_runs (
  id TEXT PRIMARY KEY,
  model_version_id TEXT NOT NULL REFERENCES model_versions(id),
  dataset_hash TEXT NOT NULL,
  started_at REAL NOT NULL,
  completed_at REAL,
  status TEXT NOT NULL,
  metrics_json TEXT,
  reviewer_decision TEXT
);

CREATE TABLE IF NOT EXISTS command_tombstones (
  id TEXT PRIMARY KEY,
  device_id TEXT,
  immutable_hash TEXT,
  terminal_state TEXT NOT NULL,
  completed_at REAL NOT NULL
);

CREATE TABLE IF NOT EXISTS snapshots (
  id TEXT PRIMARY KEY,
  kind TEXT NOT NULL CHECK (kind IN ('search','player','view')),
  device_id TEXT,
  command_id TEXT REFERENCES commands(id),
  app_id TEXT NOT NULL,
  video_id TEXT,
  generation TEXT NOT NULL,
  observed_at REAL NOT NULL,
  expires_at REAL NOT NULL,
  payload_json TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_snapshots_generation ON snapshots(generation);

CREATE TABLE IF NOT EXISTS confirmations (
  id TEXT PRIMARY KEY,
  command_id TEXT NOT NULL UNIQUE REFERENCES commands(id) ON DELETE CASCADE,
  device_id TEXT,
  account_id TEXT,
  frozen_plan_json TEXT NOT NULL,
  plan_hash TEXT NOT NULL,
  expires_at REAL NOT NULL,
  consumed_at REAL,
  status TEXT NOT NULL CHECK (status IN ('pending','consumed','expired','canceled')),
  result_json TEXT
);

CREATE TABLE IF NOT EXISTS account_rate_limits (
  account_id TEXT PRIMARY KEY,
  reserved_command_id TEXT,
  reserved_step_index INTEGER,
  last_attempt_at REAL,
  blocked_until REAL
);

CREATE TABLE IF NOT EXISTS device_sessions (
  boot_id TEXT PRIMARY KEY,
  device_id TEXT NOT NULL REFERENCES devices(id),
  firmware_version TEXT NOT NULL,
  first_seen_at REAL NOT NULL,
  last_seen_at REAL NOT NULL
);
