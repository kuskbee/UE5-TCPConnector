#include "DatabaseManager.h"

//MySQL
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/exception.h>
#include <jdbc/cppconn/prepared_statement.h>

//Ini
#include "../Utils/IniParser.h"
#include <sstream>

bool DatabaseManager::Initialize(std::string& ConfigPath)
{
	// get database config
	std::string Section = "Database";
	auto DbConfig = util::IniParser::ParseSection(ConfigPath, Section);

	std::string Host = DbConfig["Host"];
	std::string Port = DbConfig["Port"];
	DBUser = DbConfig["User"];
	DBPass = DbConfig["Password"];
	Schema = DbConfig["Schema"];

	std::stringstream Url;
	Url << "tcp://" << Host << ":" << Port;
	DBUrl = Url.str();

	try
	{
		SqlDriver = get_driver_instance();

		conn.reset(SqlDriver->connect(Url.str(), DBUser, DBPass));

		conn->setSchema(Schema);
	}
	catch (sql::SQLException& e)
	{
		std::cerr << "Could not connect to database. Error: " << e.what() << std::endl;
		return false;
	}

	std::cout << "Database connection successful!" << std::endl;
	return true;
}

sql::Connection* DatabaseManager::GetConnection()
{
	// 스레드-로컬 static unique_ptr
	thread_local std::unique_ptr<sql::Connection> conn;
	if (!conn) {
		try {
			// 최초 요청 시 driver_ 로부터 커넥션 생성
			conn.reset(SqlDriver->connect(DBUrl, DBUser, DBPass));
			conn->setSchema(Schema);
		}
		catch (sql::SQLException& e) {
			std::cerr << "DB connect error in thread: " << e.what() << "\n";
			return nullptr;
		}
	}
	return conn.get();
}
