-- Device feedback overlay data. Existing feedback rows remain valid; the
-- legacy kind column is retained for compatibility.
ALTER TABLE feedback ADD COLUMN rating TEXT;
ALTER TABLE feedback ADD COLUMN reason TEXT;
ALTER TABLE feedback ADD COLUMN dismissed_by TEXT;
ALTER TABLE feedback ADD COLUMN touch_count INTEGER;
ALTER TABLE feedback ADD COLUMN countdown_at_dismiss INTEGER;
ALTER TABLE feedback ADD COLUMN context_json TEXT;
ALTER TABLE feedback ADD COLUMN source TEXT NOT NULL DEFAULT 'device';
ALTER TABLE feedback ADD COLUMN received_at REAL;

CREATE UNIQUE INDEX IF NOT EXISTS idx_feedback_command ON feedback(command_id);

CREATE TABLE IF NOT EXISTS feedback_analysis_runs (
  id TEXT PRIMARY KEY,
  created_at REAL NOT NULL,
  groups INTEGER NOT NULL,
  explicit_labels INTEGER NOT NULL,
  model TEXT,
  report_json TEXT,
  status TEXT NOT NULL
);
