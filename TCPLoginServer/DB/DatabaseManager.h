#pragma once

#include <string>
#include <jdbc/cppconn/driver.h>

struct ConnDeleter {
	void operator()(sql::Connection* p) const {
		p->close();
		delete p;
	}
};

class DatabaseManager
{
public:
	bool Initialize(std::string& ConfigPath);

	sql::Connection* GetConnection();

private:
	sql::Driver* SqlDriver;
	
	std::unique_ptr<sql::Connection, ConnDeleter> conn;

	std::string DBUser;
	std::string DBPass;
	std::string DBUrl;
	std::string Schema;
};