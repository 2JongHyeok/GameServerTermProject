-- Resets every seeded account to its initial state so that measurements start from the same conditions.
-- The server overwrites level, exp, and position on disconnect (SaveData), so run this before re-measuring.
-- Requires schema.sql to have been run first.
--
-- Example: sqlcmd -S localhost\SQLEXPRESS -E -C -i DB\reset_seed.sql

USE GS_Term_Project;
GO

UPDATE dbo.UserData
SET [level] = 1, [exp] = 0, x = 900, y = 1000;
GO
