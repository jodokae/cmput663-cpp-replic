#include "ajp.h"
APLOG_USE_MODULE(proxy_ajp);
apr_status_t ajp_ilink_send(apr_socket_t *sock, ajp_msg_t *msg) {
char *buf;
apr_status_t status;
apr_size_t length;
ajp_msg_end(msg);
length = msg->len;
buf = (char *)msg->buf;
do {
apr_size_t written = length;
status = apr_socket_send(sock, buf, &written);
if (status != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, status, NULL, APLOGNO(01029)
"ajp_ilink_send(): send failed");
return status;
}
length -= written;
buf += written;
} while (length);
return APR_SUCCESS;
}
static apr_status_t ilink_read(apr_socket_t *sock, apr_byte_t *buf,
apr_size_t len) {
apr_size_t length = len;
apr_size_t rdlen = 0;
apr_status_t status;
while (rdlen < len) {
status = apr_socket_recv(sock, (char *)(buf + rdlen), &length);
if (status == APR_EOF)
return status;
else if (APR_STATUS_IS_EAGAIN(status))
continue;
else if (status != APR_SUCCESS)
return status;
rdlen += length;
length = len - rdlen;
}
return APR_SUCCESS;
}
apr_status_t ajp_ilink_receive(apr_socket_t *sock, ajp_msg_t *msg) {
apr_status_t status;
apr_size_t hlen;
apr_size_t blen;
hlen = msg->header_len;
status = ilink_read(sock, msg->buf, hlen);
if (status != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, status, NULL, APLOGNO(01030)
"ajp_ilink_receive() can't receive header");
return (APR_STATUS_IS_TIMEUP(status) ? APR_TIMEUP : AJP_ENO_HEADER);
}
status = ajp_msg_check_header(msg, &blen);
if (status != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, APLOGNO(01031)
"ajp_ilink_receive() received bad header");
return AJP_EBAD_HEADER;
}
status = ilink_read(sock, msg->buf + hlen, blen);
if (status != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, status, NULL, APLOGNO(01032)
"ajp_ilink_receive() error while receiving message body "
"of length %" APR_SIZE_T_FMT,
hlen);
return AJP_EBAD_MESSAGE;
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, NULL, APLOGNO(01033)
"ajp_ilink_receive() received packet len=%" APR_SIZE_T_FMT
"type=%d",
blen, (int)msg->buf[hlen]);
return APR_SUCCESS;
}