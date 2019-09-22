/** @file http.c
 *  @brief Simple HTTP upload service
 *
 *  Saves files sent to the /upload endpoid
 * 
 *  Usage example:
 *       curl --data "name=saved-file-name" --data @./file-to-upload.wasm http://localhost:8021/upload
 *
 *  NOTES: saved-file-name is appended with ".wasm"; files are saved to the
 *  upload folder in config.ini
 *
 *  @author Nuno Pereira
 *  @date July, 2019
 */
#include "ini.h"
#include "mongoose.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

static const char *s_http_port = "8021";
static struct mg_serve_http_opts s_http_server_opts;

static const char g_config_file_path[] = "config.ini";

#define STR_MAXLEN 100

struct {
  char http_port[STR_MAXLEN];
  char http_enable_directory_listing[STR_MAXLEN];

  char upload_folder[STR_MAXLEN];
} g_config;

static int conf_handler(void *user, const char *section, const char *name,
                        const char *value) {
  if (user != NULL)
    return -1;
#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
  if (MATCH("http-upload", "port")) {
    strncpy(g_config.http_port, value, sizeof(g_config.http_port));
    printf("http_port = %s\n", g_config.http_port);
  } else if (MATCH("http-upload", "enable-directory-listing")) {
    strncpy(g_config.http_enable_directory_listing, value,
            sizeof(g_config.http_enable_directory_listing));
    printf("http_enable_directory_listing = %s\n",
           g_config.http_enable_directory_listing);
  } else if (MATCH("http-upload", "upload-folder")) {
    strncpy(g_config.upload_folder, value, sizeof(g_config.upload_folder));
    printf("upload_folder= %s\n", g_config.upload_folder);
  } else {
    return 0; /* unknown section/name, error */
  }
  return 1;
}

int read_config() {
  if (ini_parse(g_config_file_path, conf_handler, NULL) < 0) {
    printf("Can't load 'config.ini'\n");
    return -1;
  }
  return 0;
}

static void file_upload(struct mg_connection *nc, struct http_message *hm) {
  static char ok_chars[] = "abcdefghijklmnopqrstuvwxyz"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "1234567890_-.@";
  FILE *fp;
  int ret;
  char arg_name[100] = {0}, filename[200] = {0};
  uint32_t file_len = 0;

  ret = mg_get_http_var(&hm->body, "name", arg_name, sizeof(arg_name) - 1);
  if (ret < 0)
    mg_printf(nc, "%s",
              "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");

  uint32_t param_len = strlen(arg_name) + strlen("name") + 2;
  char *file_buf = (char *)hm->body.p + param_len;

  file_len = (uint32_t)hm->body.len - param_len;

  /* do sanitization on name argument received */
  char *cp = arg_name; /* Cursor into string */
  const char *end = arg_name + strlen(arg_name);
  for (cp += strspn(cp, ok_chars); cp != end; cp += strspn(cp, ok_chars)) {
    *cp = '_';
  }

  snprintf(filename, sizeof(filename), "%s/%s.wasm", g_config.upload_folder,
           arg_name);

  printf("http_upload: %s; len:%u\n", filename, file_len);

  fp = fopen(filename, "w");
  fwrite(file_buf, 1, file_len, fp);

  fclose(fp);

  mg_printf(nc, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");

  /*  send response as a JSON object */
  mg_printf_http_chunk(nc, "{ \"result\": \"ok\" }");
  mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
  struct http_message *hm = (struct http_message *)ev_data;

  if (ev == MG_EV_HTTP_REQUEST) {
    if (mg_vcmp(&hm->uri, "/upload") == 0 &&
        mg_vcmp(&hm->method, "POST") == 0) {
      file_upload(nc, hm);
    }
  }
}

int main(void) {
  struct mg_mgr mgr;
  struct mg_connection *c;

  if (read_config() < 0)
    return -1;

  mg_mgr_init(&mgr, NULL);
  c = mg_bind(&mgr, g_config.http_port, ev_handler);
  if (c == NULL) {
    fprintf(stderr, "Cannot start server on port %s\n", s_http_port);
    exit(EXIT_FAILURE);
  }

  s_http_server_opts.document_root =
      g_config.upload_folder; // Serve dowload files folder
  s_http_server_opts.enable_directory_listing =
      g_config.http_enable_directory_listing;

  // Set up HTTP server parameters
  mg_set_protocol_http_websocket(c);

  printf("Starting web server on port %s\n", g_config.http_port);
  for (;;) {
    mg_mgr_poll(&mgr, 1000);
  }
  mg_mgr_free(&mgr);

  return 0;
}