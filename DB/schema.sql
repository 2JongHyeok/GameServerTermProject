-- GS_Term_Project database bootstrap script
-- Creates the table and the two stored procedures that Server/server.cpp calls:
--   EXEC SearchClient <id>                        : returns one row as name, level, exp, x, y
--   EXEC SaveData <id>, <level>, <exp>, <x>, <y>  : updates that row
-- Safe to run repeatedly: existing objects and rows are left alone.
--
-- Example: sqlcmd -S localhost\SQLEXPRESS -E -C -i DB\schema.sql

IF DB_ID(N'GS_Term_Project') IS NULL
	CREATE DATABASE GS_Term_Project;
GO

USE GS_Term_Project;
GO

-- Player account table
IF OBJECT_ID(N'dbo.UserData', N'U') IS NULL
	CREATE TABLE dbo.UserData (
		id		INT				NOT NULL PRIMARY KEY,	-- CS_LOGIN_PACKET.id
		name	NVARCHAR(20)	NOT NULL,				-- matches NAME_SIZE (20)
		[level]	INT				NOT NULL DEFAULT 1,
		[exp]	INT				NOT NULL DEFAULT 0,
		x		INT				NOT NULL,
		y		INT				NOT NULL
	);
GO

-- Account lookup at login. No row means the server treats the login as failed.
-- The server binds the result by column position, so do not reorder the SELECT list.
CREATE OR ALTER PROCEDURE dbo.SearchClient
	@id INT
AS
BEGIN
	SET NOCOUNT ON;
	SELECT name, [level], [exp], x, y
	FROM dbo.UserData
	WHERE id = @id;
END
GO

-- Save state on disconnect
CREATE OR ALTER PROCEDURE dbo.SaveData
	@id INT, @level INT, @exp INT, @x INT, @y INT
AS
BEGIN
	SET NOCOUNT ON;
	UPDATE dbo.UserData
	SET [level] = @level, [exp] = @exp, x = @x, y = @y
	WHERE id = @id;
END
GO

-- Seed accounts. The stress test logs in with ids starting at 1, so create MAX_USER (10000) rows up front.
-- (900, 1000) is a walkable tile (value 50) in mymap.txt; the server does not validate positions read from the DB.
WITH n AS (
	SELECT 1 AS v
	UNION ALL
	SELECT v + 1 FROM n WHERE v < 10000
)
INSERT INTO dbo.UserData (id, name, [level], [exp], x, y)
SELECT v, CAST(v AS NVARCHAR(20)), 1, 0, 900, 1000
FROM n
WHERE NOT EXISTS (SELECT 1 FROM dbo.UserData u WHERE u.id = n.v)
OPTION (MAXRECURSION 0);
GO
