#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mysql.h>

#include "database.h"

enum donor_type { FOODBANK = 0, PEOPLE, ALL };

struct giveaway_data {
	int id;
	char name[20];
	double lat;
	double lng;
	char start[10];
	char end[10];
};

static void	show_sql_error(MYSQL *sql);

static void
show_sql_error(MYSQL *sql)
{

	printf("Error(%d) [%s] \"%s\"\n", mysql_errno(sql), mysql_sqlstate(sql),
	    mysql_error(sql));

	mysql_close(sql);
	mysql_thread_end();
}

/*
 * Sets up the database connection. Connects to local mariadb database
 * exiting if there's and error.
 */
void
init_database()
{
	MYSQL *mysql;
	char *query;

	if (mysql_library_init(0, NULL, NULL)) {
		printf("Unable to initialize MySQL library.\n");
		exit(1);
	}

	mysql = mysql_init(NULL);

	if (!mysql_real_connect(mysql, "localhost", "root", NULL, NULL,
		0, "/run/mysqld/mysqld.sock", 0)) {
		printf("Unable to connect to MariaDB server\n");
		show_sql_error(mysql);
		exit(1);
	}

	query = "CREATE DATABASE IF NOT EXISTS ffd_db;";

	if (mysql_real_query(mysql, query, strlen(query))) {
		printf("Unable to query/create database \"ffd_db\"\n");
		show_sql_error(mysql);
		exit(1);
	}

	query = "USE ffd_db;";

	if (mysql_real_query(mysql, query, strlen(query))) {
		printf("Unable to switch to database \"ffd_db\"\n");
		show_sql_error(mysql);
		exit(1);
	}

	query = "CREATE TABLE IF NOT EXISTS giveaways ("
	    "id INT UNSIGNED NOT NULL AUTO_INCREMENT,"
	    "name VARCHAR(20) NOT NULL,"
	    "desc VARCHAR(40) NOT NULL,"
	    "lat DOUBLE NOT NULL,"
	    "long DOUBLE NOT NULL,"
	    "type ENUM('foodbank', 'people', 'all') NOT NULL,"
	    "start DATE NOT NULL,"
	    "end DATE NOT NULL,"
	    "PRIMARY KEY (id));";

	if (mysql_real_query(mysql, query, strlen(query))) {
		printf("Unable to query/create table \"giveaways\"\n");
		show_sql_error(mysql);
		exit(1);
	}

	query = "CREATE TABLE IF NOT EXISTS food ("
	    "giveaway INT UNSIGNED NOT NULL,"
	    "name VARCHAR(20) NOT NULL,"
	    "amount VARCHAR(40) NOT NULL,"
	    "PRIMARY KEY (giveaway));";

	if (mysql_real_query(mysql, query, strlen(query))) {
		printf("Unable to query/create table \"food\"\n");
		show_sql_error(mysql);
		exit(1);
	}

	mysql_close(mysql);
	mysql_thread_end();
}

void
close_database()
{
	mysql_library_end();
}

int
process_get_query(char *msg, char *newmsg)
{
	char msgln[256];
	char *pch;
	int length, type;
	double lat, lng;
	char query[300];
	MYSQL *mysql;
	MYSQL_RES *result;
	MYSQL_ROW row;

	pch = strpbrk(msg, " ");
	if (pch == NULL)
		return 0;
	pch++;

	length = strcspn(pch, " ");
	if (length > sizeof(msgln))
		return 0;

	strncpy(msgln, pch, length);
	msgln[length] = '\0';

	pch = strpbrk(msgln, "?");
	if (pch == NULL)
		return 0;
	pch++;

	length = strcspn(pch, "#");
	pch[length] = '\0';

	type = 0;
	lat = 0.0;
	lng = 0.0;

	pch = strtok(pch, "&");
	while (pch != NULL) {
		if (strncmp(pch, "type=", 5) == 0)
			type = strtol(pch + 5, NULL, 0);
		else if (strncmp(pch, "lat=", 4) == 0)
			lat = strtod(pch + 4, NULL);
		else if (strncmp(pch, "lng=", 4) == 0)
			lng = strtod(pch + 5, NULL);
		pch = strtok(NULL, "&");
	}

	if (type == 0 || lat == 0.0 || lng == 0.0)
		return 0;

	mysql = mysql_init(NULL);

	if (!mysql_real_connect(mysql, "localhost", "root", NULL, "ffd_db", 0,
		"/run/mysqld/mysqld.sock", 0)) {
		printf("Unable for thread to connect to MariaDB database\n");
		show_sql_error(mysql);
		return 0;
	}

	if (!(type == 1 || type == 2))
		return 0;

	sprintf(query, "SELECT id, name, desc, lat, long, end FROM giveaways "
	    "WHERE (type='all' OR type='%s') "
	    "AND start < CURRENT_DATE AND end < CURRENT_DATE "
	    "ORDER BY ACOS("
	    "COS(RADIANS(%lg))) * COS(RADIANS(lat)) * "
	    "COS(RADIANS(long) - RADIANS(%lg)) * "
	    "SIN(RADIANS(lat))) "
	    "LIMIT 0, 5;",
	    type == 1 ? "foodbank" : "people", lat, lng);

	if (mysql_real_query(mysql, query, strlen(query))) {
		printf("Unable for thread to query database\n");
		show_sql_error(mysql);
		return 0;
	}

	result = mysql_store_result(mysql);

	if (result == NULL) {
		printf("Unable to store query results\n");
		show_sql_error(mysql);
		return 0;
	}

	newmsg = "{\"locations\":[";
	pch = newmsg + strlen(newmsg);
	while ((row = mysql_fetch_row(result))) {
		pch += sprintf("{\"name\":%s,\"desc\":%s,", row[1], row[2]);
		pch += sprintf("\"lat\":%s,\"lng\":%s,", row[2], row[3]);
	}

	mysql_free_result(result);

	mysql_close(mysql);
	mysql_thread_end();
}

int
process_post_query(char *msg, char *newmsg)
{

}
