#include "uri_parse.h"


// HTTP Processing functions
void process_startline(char *request, char **method, char **path, char **version) {
    *method  = strtok(request, " ");
    *path    = strtok(NULL, " ");
    *version = strtok(NULL, "\r\n");
}


void parse_http_uri(const char *path, char **filename, char **filetype) {
    // return filename and file extension type

    char path_string[strlen(path) + 1];
    char *path_tok, *path_tok_prev;

    strcpy(path_string, path); // copy of path string for parsing out the filename

    path_tok = strtok(path_string, "/");

    do {
        path_tok_prev = path_tok;
    } while ((path_tok = strtok(NULL, "/")) != NULL);


    *filename = malloc(sizeof(path_tok_prev)); // todo: remove malloc. use pre-allocated array on stack instead
    strcpy(*filename, path_tok_prev);

    *filetype = malloc(sizeof(path_tok_prev)); // todo: remove malloc. use pre-allocated array on stack instead
    strcpy(*filetype, path_tok_prev);
    strtok(*filetype, ".");
    *filetype = strtok(NULL, ".");

    //printf("[INFO]: filename: %s, filetype: %s\n", *filename, *filetype);
}




int content_type_lookup(char *content_type, char *filetype) {
    // input:
    // -    content_type - a pointer to a destination buffer on the stack
    // to be filled with a content-type string
    //
    // -    filetype - a file's file extension type. if NULL (for an empty string)
    // then the associated content-type is 'application/octet-stream', otherwise
    // directly pattern matches the extension for a content-type.
    //
    // output:
    // - content_type (char *)
    // - video_transfer (int)


    char * content_type_tmp;
    int video_transfer = 0;

    if      (filetype == NULL)               {content_type_tmp = "application/octet-stream";}
    else if (strcmp(filetype, "txt" ) == 0)  {content_type_tmp = "text/plain";}
    else if (strcmp(filetype, "css" ) == 0)  {content_type_tmp = "text/css";}
    else if (strcmp(filetype, "htm" ) == 0)  {content_type_tmp = "text/html";}
    else if (strcmp(filetype, "html") == 0)  {content_type_tmp = "text/html";}
    else if (strcmp(filetype, "jpg" ) == 0)  {content_type_tmp = "image/jpeg";}
    else if (strcmp(filetype, "jpeg") == 0)  {content_type_tmp = "image/jpeg";}
    else if (strcmp(filetype, "png" ) == 0)  {content_type_tmp = "image/png";}
    else if (strcmp(filetype, "js"  ) == 0)  {content_type_tmp = "application/javascript";}
    else if (strcmp(filetype, "mp4" ) == 0)  {content_type_tmp = "video/webm"; video_transfer = 1;}
    else if (strcmp(filetype, "webm") == 0)  {content_type_tmp = "video/webm"; video_transfer = 1;}
    else if (strcmp(filetype, "ogg" ) == 0)  {content_type_tmp = "video/webm"; video_transfer = 1;}
    else                                     {exit(1);}


    strcpy(content_type, content_type_tmp);
    return video_transfer;
}
