set -e
mkdir -p $1/www/htdocs
mkdir -p $1/www/acme
mkdir -p $1/www/logs
mkdir -p $1/etc/acme
mkdir -p $1/etc/ssl/acme/private
cat <<END > $1/etc/httpd.conf
chroot "$1/www"
server "default" {
	listen on "*" port 80
	location "/.well-known/acme-challenge/*" {
		root "/acme"
		root strip 2
	}
}
END
