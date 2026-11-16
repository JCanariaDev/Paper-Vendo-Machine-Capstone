-- Adds machine logging for checkout reservation failures and timeouts.
-- Run this once in Supabase SQL Editor on an existing database.

CREATE OR REPLACE FUNCTION machine_record_system_event(
    p_level TEXT,
    p_source TEXT,
    p_event_type TEXT,
    p_message TEXT,
    p_transaction_id UUID DEFAULT NULL,
    p_tr_number TEXT DEFAULT NULL,
    p_metadata JSONB DEFAULT '{}'::jsonb
)
RETURNS VOID
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = public
AS $$
BEGIN
    INSERT INTO machine_logs (
        level, source, event_type, message, transaction_id, tr_number, metadata
    ) VALUES (
        COALESCE(NULLIF(p_level, ''), 'ERROR'),
        COALESCE(NULLIF(p_source, ''), 'SYSTEM'),
        COALESCE(NULLIF(p_event_type, ''), 'SYSTEM_ERROR'),
        COALESCE(NULLIF(p_message, ''), 'Machine event recorded'),
        p_transaction_id,
        p_tr_number,
        COALESCE(p_metadata, '{}'::jsonb)
    );
END;
$$;

GRANT EXECUTE ON FUNCTION machine_record_system_event(TEXT, TEXT, TEXT, TEXT, UUID, TEXT, JSONB)
TO anon, authenticated, service_role;
