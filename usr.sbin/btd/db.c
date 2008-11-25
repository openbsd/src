#include <sys/stat.h>

#include <netbt/hci.h>

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "btd.h"

typedef enum {
	DB_DEVINFO,
	DB_LINK_KEY
} db_type;

typedef struct {
	bdaddr_t addr;
	db_type type;
} db_key;

int db_put(struct btd_db *, db_key *, const void *, size_t);
int db_get_raw(struct btd_db *, db_key *, void **, size_t *);
int db_get_exact(struct btd_db *, db_key *, void *, size_t);

void
db_open(const char *file, struct btd_db *db)
{
	assert(db->dbh == NULL);

	db->dbh = dbopen(file, O_CREAT | O_EXLOCK | O_RDWR,
	    S_IRUSR | S_IWUSR, DB_HASH, NULL);
	if (db->dbh == NULL)
		fatal(file);
}

int
db_put(struct btd_db *db, db_key *key, const void *data, size_t size)
{
	DB *dbh = db->dbh;
	DBT dbk, dbd;

	memset(&dbk, 0, sizeof(dbk));
	dbk.size = sizeof(db_key);
	dbk.data = (void *)key;
	memset(&dbd, 0, sizeof(dbd));
	dbd.size = size;
	dbd.data = (void*)data;

	if (dbh->put(dbh, &dbk, &dbd, 0)) {
		log_warn("db_put");
		return -1;
	}

	if (dbh->sync(dbh, 0)) {
		log_warn("db_sync");
		return -1;
	}

	return 0;
}

int
db_get_raw(struct btd_db *db, db_key *key, void **data, size_t *size)
{
	DB *dbh = db->dbh;
	DBT dbk, dbd;
	int res;

	memset(&dbk, 0, sizeof(dbk));
	dbk.size = sizeof(db_key);
	dbk.data = (void *)key;
	memset(&dbd, 0, sizeof(dbd));

	if ((res = dbh->get(dbh, &dbk, &dbd, 0)) != 0) {
		if (res == -1)
			log_warn("db_get");
		return res;
	}

	*data = dbd.data;
	*size = dbd.size;
	return 0;
}

int
db_get_exact(struct btd_db *db, db_key *key, void *data, size_t size)
{
	void *d;
	size_t dlen;
	int res;

	if ((res = db_get_raw(db, key, &d, &dlen)) == 0) {
		if (dlen != size) {
			log_warnx("data size mismatch for key type %#x"
			    " (%u != %u)", key->type, dlen, size);
			return -1;
		}
		memcpy(data, d, size);
	}

	return res;
}

int
db_put_link_key(struct btd_db *db, const bdaddr_t *addr,
    const uint8_t *link_key)
{
	db_key key;

	memset(&key, 0, sizeof(key));
	bdaddr_copy(&key.addr, addr);
	key.type = DB_LINK_KEY;

	return db_put(db, &key, link_key, HCI_KEY_SIZE);
}

int
db_get_link_key(struct btd_db *db, const bdaddr_t *addr, uint8_t *link_key)
{
	db_key key;

	memset(&key, 0, sizeof(key));
	bdaddr_copy(&key.addr, addr);
	key.type = DB_LINK_KEY;

	return db_get_exact(db, &key, link_key, HCI_KEY_SIZE);
}

int
db_put_devinfo(struct btd_db *db, const bdaddr_t *addr,
    const struct bt_devinfo *info)
{
	db_key key;
	void *data;
	size_t size;
	int res;

	memset(&key, 0, sizeof(key));
	bdaddr_copy(&key.addr, addr);
	key.type = DB_DEVINFO;

	if (devinfo_store(info, &data, &size))
		return -1;

	res = db_put(db, &key, data, size);
	free(data);
	return res;
}

int
db_get_devinfo(struct btd_db *db, const bdaddr_t *addr,
    struct bt_devinfo *info)
{
	db_key key;
	void *data;
	size_t size;
	int res;

	memset(&key, 0, sizeof(key));
	bdaddr_copy(&key.addr, addr);
	key.type = DB_DEVINFO;

	if ((res = db_get_raw(db, &key, &data, &size)) == 0 &&
	    devinfo_load(info, data, size))
		res = -1;

	return res;
}

void
db_dump(struct btd_db *db)
{
	struct bt_devinfo info;
	DB *dbh = db->dbh;
	DBT dbk, dbd;

	if (dbh->seq(dbh, &dbk, &dbd, R_FIRST) != 0)
		return;
	do {
		db_key *key = dbk.data;

		if (dbk.size != sizeof(db_key))
			fatalx("invalid db key");

		switch (key->type) {
		case DB_LINK_KEY:
			log_info("%s link_key", bt_ntoa(&key->addr, NULL));
			break;
		case DB_DEVINFO:
			log_info("%s devinfo", bt_ntoa(&key->addr, NULL));
			if (db_get_devinfo(db, &key->addr, &info) == 0) {
				devinfo_dump(&info);
				devinfo_unload(&info);
			}
			break;
		default:
			fatalx("invalid db key type");
		}
	} 
	while (dbh->seq(dbh, &dbk, &dbd, R_NEXT) == 0);
}
