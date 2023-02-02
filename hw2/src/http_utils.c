#include "http_utils.h"

void generate_timestamp(char *buf) {
    //time_t now = time(0);
    //struct tm tm_ = *gmtime(&now);
    //strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm_);

    time_t current_time; // the date string

    struct tm *time_info;
    char time_string[100];

    time(&current_time);
    time_info = gmtime(&current_time);
    strftime(time_string, sizeof(time_string), "%a, %d %b %Y %H:%M:%S GMT", time_info);

    //printf("NOTICE: %s\n", time_string);

    strcpy(buf, time_string);

}

void transfer_file_chunk(char *og_req_buffer, char *file_path, int socket_fd,
                    char *content_type) {
    // seperate http response file tranfer routine for video-type files

    // open the file
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        // TODO: insert 500 http response here?
        perror("Error opening video file");
        close(socket_fd);
        exit(1);
    }

    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    char headers[1000];
    char last_modified[100];
    struct tm timeinfo;
    struct stat stat_buf;
    fstat(fd, & stat_buf);
    localtime_r( & stat_buf.st_mtime, & timeinfo);
    strftime(last_modified, sizeof(last_modified), "%a, %d %b %Y %H:%M:%S %Z", & timeinfo);

    char date_timestamp[200];
    generate_timestamp(date_timestamp);

    // check for the Range field in the headers
    if (strstr(og_req_buffer, "Range:")) {
        char * range_start, *range_end;
        // extract the range values from the headers
        char *range_header = strstr(og_req_buffer, "Range:");
        // sscanf(buffer, "Range: bytes=%ld-%ld", & range_start, & range_end);
        char *range_start_str, *range_end_str;

        range_start_str = strchr(range_header, '=') + 1;
        range_end_str = strchr(range_start_str, '-');
        if (range_end_str && isdigit((unsigned char)*(range_end_str+1))) {
            *range_end_str = '\0'; // replace '-' with null character
            range_start = strtol(range_start_str, NULL, 10);
            range_end = strtol(range_end_str + 1, NULL, 10);
        } else {
            range_start = strtol(range_start_str, NULL, 10);
            range_end = file_size - 1;
        }
        // check if range starts from 0
        if (range_start >= 0 && range_start < file_size) {
            if (range_end >= file_size) {
                range_end = file_size - 1; // adjust end range to the end of the file
            }

            if (range_start == 0) {
                // send "206 Partial Content" response with appropriate Content-Range header
                sprintf(headers, "HTTP/1.1 206 Partial Content\r\n"
                                "Content-Range: bytes 0-%ld/%ld\r\n"
                                "Content-Type: %s\r\nLast-Modified: %s\r\n"
                                "Date: %s\r\n"
                                "Connection: Keep-Alive\r\n\r\n",
                                range_end, file_size, content_type, last_modified, date_timestamp);
            } else {
                // send "206 Partial Content" response with appropriate Content-Range header
                sprintf(headers, "HTTP/1.1 206 Partial Content\r\n"
                             "Content-Range: bytes %ld-%ld/%ld\r\n"
                             "Content-Type: %s\r\nLast-Modified: %s\r\n"
                             "Date: %s\r\n"
                             "Connection: Keep-Alive\r\n\r\n",
                             range_start, range_end, file_size, content_type, last_modified, date_timestamp);
            }

            send(socket_fd, headers, strlen(headers), 0);
            // send the requested range of bytes
            lseek(fd, range_start, SEEK_SET);

            char buffer[4096];
            ssize_t bytes_read, bytes_to_read;

            bytes_to_read = range_end - range_start + 1;

            while (bytes_to_read > 0) {
                if (sizeof(buffer) > bytes_to_read) {
                    bytes_read = read(fd, buffer, bytes_to_read);
                } else {
                    bytes_read = read(fd, buffer, sizeof(buffer));
                }

                send(socket_fd, buffer, bytes_read, 0);
                bytes_to_read -= bytes_read;
            }
        } else {
            // send "416 Range Not Satisfiable" response if range is not valid
            sprintf(headers, "HTTP/1.1 416 Range Not Satisfiable\r\n"
            "Content-Range: bytes */%ld\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n\r\n"
            "Date: %s\r\n"
            "<html><body>416 Range Not Satisfiable</body></html>\r\n", file_size, date_timestamp);
            send(socket_fd, headers, strlen(headers), 0);
        }
    } else {
        // send "200 OK" response if no range is specified
        sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n"
                        "Content-Length: %ld\r\nLast-Modified: %s\r\n"
                        "Date: %s\r\n"
                        "Connection: Keep-Alive\r\n\r\n",
                        content_type, file_size, last_modified, date_timestamp);

        send(socket_fd, headers, strlen(headers), 0);

        char buffer[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            send(socket_fd, buffer, bytes_read, 0);
        }
    }

    close(fd);
}


void transfer_standard_file(char *file_path, int socket_fd, char *content_type) {
    // transfers an entire file located at file_path through the socket
    // at socket_fd in a single http response.
    //
    // the http's Content-Type header field is parameterised by 'content_type'
    // string


    int fd = open(file_path, O_RDONLY);

    if (fd == -1) {

        perror("Error opening text file");

        char* message = "HTTP/1.1 404 Not Found\r\n" // TODO: maybe this should be a different error message
                        "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                        "<html><body>404 Not Found</body></html>\r\n";

        send(socket_fd, message, strlen(message), 0);
        close(socket_fd);
        exit(1);
    }

    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    char headers[1000];
    char last_modified[100];
    char buffer[4096];

    struct tm timeinfo;
    struct stat stat_buf;

    fstat(fd, &stat_buf);
    localtime_r(&stat_buf.st_mtime, &timeinfo);
    strftime(last_modified, sizeof last_modified, "%a, %d %b %Y %H:%M:%S %Z", & timeinfo);

    sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n"
                     "Content-Length: %ld\r\nLast-Modified: %s\r\n\r\n"
                     "Connection: Keep-Alive\r\n\r\n", content_type, file_size, last_modified);

    send(socket_fd, headers, strlen(headers), 0);

    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof buffer)) > 0) {
        send(socket_fd, buffer, bytes_read, 0);
    }
}



void send_http_200(int connection_fd) {
    char message[200];
    char date_timestamp[100];

    generate_timestamp(date_timestamp);

    sprintf(message,"HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html; charset=UTF-8\r\n"
                     // note: we place double <CR> <LF> here since
                     // the following part of the response is the content payload
                    "Date: %s\r\n\r\n"
                    "<html><body>Hello World!</body></html>\r\n",
                    date_timestamp);

    send(connection_fd, message, strlen(message), 0);

    close(connection_fd);
}