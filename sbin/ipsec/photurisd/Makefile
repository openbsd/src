PROG=   photurisd
LDADD=	-lgmp -ldes
DPADD=	${LIBGMP} ${LIBDES}
SRCS=   photuris_cookie_request.c photuris_cookie_response.c \
	photuris_value_request.c photuris_value_response.c \
	photuris_identity_request.c photuris_identity_response.c \
	photuris_spi_needed.c photuris_spi_update.c photuris_error_message.c \
	photuris_packet_encrypt.c \
	handle_cookie_request.c handle_value_request.c \
	handle_cookie_response.c handle_value_response.c \
	handle_identity_request.c handle_identity_response.c \
	handle_spi_needed.c handle_spi_update.c\
	handle_bad_cookie.c handle_resource_limit.c \
	handle_verification_failure.c handle_message_reject.c \
	errlog.c config.c scheme.c schedule.c server.c \
	buffer.c compute_secrets.c cookie.c exchange.c identity.c \
	modulus.c spi.c state.c validity.c attributes.c \
	photurisd.c packet.c api.c kernel.c

CFLAGS+= -I. -DIPSEC -DDEBUG
MAN= photurisd.8

.include <bsd.prog.mk>
