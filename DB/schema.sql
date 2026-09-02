-- GS_Term_Project 데이터베이스 초기화 스크립트
-- Server/server.cpp 가 호출하는 저장 프로시저 두 개와 그 테이블을 만든다.
--   EXEC SearchClient <id>                        : name, level, exp, x, y 순서로 한 행 반환
--   EXEC SaveData <id>, <level>, <exp>, <x>, <y>  : 해당 행 갱신
-- 여러 번 실행해도 안전하다. 이미 있는 객체와 행은 건너뛴다.
--
-- 실행 예 : sqlcmd -S localhost\SQLEXPRESS -E -C -i DB\schema.sql

IF DB_ID(N'GS_Term_Project') IS NULL
	CREATE DATABASE GS_Term_Project;
GO

USE GS_Term_Project;
GO

-- 플레이어 계정 테이블
IF OBJECT_ID(N'dbo.UserData', N'U') IS NULL
	CREATE TABLE dbo.UserData (
		id		INT				NOT NULL PRIMARY KEY,	-- CS_LOGIN_PACKET.id
		name	NVARCHAR(20)	NOT NULL,				-- NAME_SIZE(20) 에 맞춤
		[level]	INT				NOT NULL DEFAULT 1,
		[exp]	INT				NOT NULL DEFAULT 0,
		x		INT				NOT NULL,
		y		INT				NOT NULL
	);
GO

-- 로그인 시 계정 조회. 행이 없으면 서버는 로그인 실패로 처리한다.
-- 서버가 결과를 컬럼 번호로 바인딩하므로 SELECT 컬럼 순서를 바꾸면 안 된다.
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

-- 접속 종료 시 상태 저장
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

-- 초기 계정 시드. 스트레스 테스트가 id 를 1부터 순서대로 보내므로 MAX_USER(10000) 개를 미리 만든다.
-- 시작 좌표 (900, 1000) 은 mymap.txt 에서 걸을 수 있는 칸(50) 이다. 서버는 DB 좌표를 검증하지 않는다.
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
