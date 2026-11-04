-- Add view-only advisor accounts without rebuilding tables or touching data.
-- Run once on the existing production database before deploying the viewer API.

BEGIN;

ALTER TABLE public.admins
  DROP CONSTRAINT IF EXISTS admins_role_check;

ALTER TABLE public.admins
  ADD CONSTRAINT admins_role_check
  CHECK (role IN ('superadmin', 'staff', 'viewer'));

COMMIT;
