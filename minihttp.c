#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <pthread.h>
#include <errno.h>

#define SERVER_PORT 8080

static int debug = 1;


inline void * do_http_request(void * pclient_sock);
inline int get_line(int sock, char *buf, int size);
inline void do_http_response(int client_sock, const char *path);
inline void do_http_response_sold(int client_sock);
inline void not_found(int client_sock); // 404
inline void send_headers(int client_sock, FILE *fp);
inline void send_body(int client_sock, FILE *fp);
inline void iner_error(int client_sock);
inline void bad_request(int client_sock);
inline void unimplemented(int client_sock); // 500


/* TODO*/
//处理HTTP请求中的缓存控制
inline void handle_cache_control(int client_sock, const char *path);
// 能够处理H TTP请求中的负载均衡
inline void handle_load_balancing(int client_sock, const char *path);
//能够处理HTTP请求中的加密和解密
inline void handle_encryption(int client_sock, const char *path);
//能够处理HTTP请求中的数据压缩和解压缩
inline void handle_compression(int client_sock, const char *path);
//能够处理HTTP请求中的文件上传和下载
inline void handle_file_transfer(int client_sock, const char *path);
//能够处理HTTP请求中的重定向和错误处理
inline void handle_redirect_error(int client_sock, const char *path);
//8. 能够处理HTTP请求中的Session信息
inline void handle_session(int client_sock, const char *path);
//能够处理HTTP请求中的Cookie信息  
inline void handle_cookie(int client_sock, const char *path);
//能够处理HTTP请求中的GET和POST方法
inline void handle_get_post(int client_sock, const char *path);
 
  
void bad_request(int client_sock)
{
    // 400
    const char *reply = "HTTP/1.0 400 Bad Request\r\n\
    Content-Type: text/html\r\n\
    \r\n\
    <HTML>\
    <HEAD>\
    <TITLE>Bad Request</TITLE>\
    </HEAD>\
    <BODY>\
        <P>Error prohibited CGI execution.\
    </BODY>\
    </HTML>\r\n";
    int len = send(client_sock, reply, strlen(reply), 0);
    if (len < 0)
    {
        perror("send");
    }
    if (debug)
        printf("send bad request\n");
    return;
}

void iner_error(int client_sock)
{
    const char *reply = "HTTP/1.0 500 Internal Sever Error\r\n\
    Content-Type: text/html\r\n\
    \r\n\
    <HTML>\
    <HEAD>\
    <TITLE>Method Not Implemented</TITLE>\
    </HEAD>\
    <BODY>\
        <P>Error prohibited CGI execution.\
    </BODY>\
    </HTML>";

    int len = write(client_sock, reply, strlen(reply));
    if (debug)
        fprintf(stdout, reply);

    if (len <= 0)
    {
        fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }
}

void unimplemented(int client_sock)
{

    const char *reply = "HTTP/1.0 501 Not Implemented\r\n\
    Content-Type: text/html\r\n\
    \r\n\
    <HTML>\
    <HEAD>\
    <TITLE>Not Implemented</TITLE>\
    </HEAD>\
    <BODY>\
        <P>Error prohibited CGI execution.\
    </BODY>\
    </HTML>";

    int len = write(client_sock, reply, strlen(reply));
    if (debug)
        fprintf(stdout, reply);

    if (len <= 0)
    {
        fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }
}

void send_body(int client_sock, FILE *fp)
{
    char buf[1024];

    fgets(buf, sizeof(buf), fp);

    while (!feof(fp))
    {
        int len = write(client_sock, buf, strlen(buf));

        if (len < 0)
        { // 发送body 的过程中出现问题,怎么办？1.重试？ 2.
            fprintf(stderr, "send body error. reason: %s\n", strerror(errno));
            break;
        }

        if (debug)
            fprintf(stdout, "%s", buf);
        fgets(buf, sizeof(buf), fp);
    }
}

void send_headers(int client_sock, FILE *fp)
{
    struct stat st;
    if (fstat(fileno(fp), &st) == -1)
    {
        perror("fstat");
        return;
    }

    char buf[1024] = {0};
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    strcat(buf, "Content-Type: text/html; charset=utf-8\r\n");
    strcat(buf, "Server:Lucifer Web Server\r\n");
    strcat(buf, "Connection:close\r\n");
    char tmp[128];
    sprintf(tmp, "Content-Length: %ld\r\n", st.st_size);
    strcat(buf, tmp);
    if (debug)
    {
        fprintf(stdout, "send_body buf: %s\n", buf);
        fflush(stdout);
    }
    if (send(client_sock, buf, strlen(buf), 0) < 0)
    {
        sprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }
    // sendfile(client_sock, fileno(fp), NULL, st.st_size); // 发送文件内容
}

void not_found(int client_sock)
{
    const char *reply = "HTTP/1.0 404 NOT FOUND\r\n\
    Content-Type: text/html\r\n\
    \r\n\
    <HTML lang=\"zh-CN\">\r\n\
    <meta content=\"text/html; charset=utf-8\" http-equiv=\"Content-Type\">\r\n\
    <HEAD>\r\n\
    <TITLE>NOT FOUND</TITLE>\r\n\
    </HEAD>\r\n\
    <BODY>\r\n\
        <P>file not find \r\n\
        <P>The server could not fulfill your request because the resource specified is unavailable or nonexistent.\r\n\
    </BODY>\r\n\
    </HTML>";

    int len = write(client_sock, reply, strlen(reply));
    if (debug)
        fprintf(stdout, reply);

    if (len <= 0)
    {
        fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }
}

void do_http_response_sold(int client_sock)
{
    const char *main_header = "HTTP/1.0 200 OK\r\nContent-Type:text/html;charset=utf-8\r\nServer:Lucifer Web Server\r\nConnection:close\r\n";
    const char *welcome_content = "<html><head><title>Welcome</title></head><body>Welcome to my server</body></html>";

    char send_buf[64];
    int wc_len = strlen(welcome_content);
    int len = write(client_sock, main_header, strlen(main_header));

    if (debug)
        fprintf(stdout, "... do_http_response...\n");
    if (debug)
        fprintf(stdout, "write[%d]: %s", len, main_header);

    len = snprintf(send_buf, 64, "Content-Length: %d\r\n\r\n", wc_len);
    len = write(client_sock, send_buf, len);
    if (debug)
        fprintf(stdout, "write[%d]: %s", len, send_buf);

    len = write(client_sock, welcome_content, wc_len);
    if (debug)
        fprintf(stdout, "write[%d]: %s", len, welcome_content);
}

void do_http_response(int client_sock, const char *path)
{
    FILE *resource = NULL;
    resource = fopen(path, "r");
    if (resource == NULL)
    {
        not_found(client_sock);
        return;
    }
    int size = fseek(resource, 0, SEEK_END);

    // 发送http头部
    send_headers(client_sock, resource);

    // 发送http_body
    send_body(client_sock, resource);

    fclose(resource);
    return;
}

int get_line(int sock, char *buf, int size)
{

    int count = 0;
    int len = 0;
    char ch = '\0';

    while (count < (size - 1) && ch != '\n')
    {
        len = read(sock, &ch, 1);
        if (len == 1)
        {
            if (ch == '\r')
            {
                // rintf("read a \\r \n");
                //
                continue;
            }
            else if (ch == '\n')
            {
                // printf("ch is \\n \n");

                // buf[count] = '\0';
                break;
            }

            buf[count] = ch;
            count++;
        }
        else if (len == -1)
        {
            perror("read faild");
            count = 0;
            break;
        }
        else
        {
            fprintf(stderr, "client close the connect \n");
            count = 0;
            break;
        }
    }

    if (count >= 0)
        buf[count] = '\0';

    return count;
}

void * do_http_request(void * pclient_sock)
{

    int len;
    char buf[512];

    char url[256];

    char method[64];
    int client_sock = *(int *)pclient_sock;
    len =  get_line(client_sock, buf, sizeof(buf));

    // 读取客户端的请求

    // 读取请求行

   
    // printf("%s\n", buf);
    if (buf == '\0')
    {
        if (debug)
        {
            printf("No request line ,The buff is null\n");
        }
    }
    if (debug)
        printf("The len is %d\n", len);
    if (len > 0)
    {
        int i = 0, j = 0;

        while (!isspace(buf[j]) && (i < (sizeof(method) - 1)))
        {
            method[i] = buf[j];
            i++;
            j++;
        }
        method[i] = '\0';
        // printf("the method is \t:%s\n", method);
        if (strncasecmp(method, "GET", i) == 0)
        {
            // 处理GET请求
            if (debug)

                printf("method = GET\n");
            while (isspace(buf[j++])) // 跳过白空格

                i = 0;
            while (!isspace(buf[j]) && (i < (sizeof(url) - 1)))
            {

                url[i] = buf[j];
                i++;
                j++;
            }

            url[i] = '\0';

            if (debug)
                printf("The url is\t:%s\n", url);

            do
            {
                len = get_line(client_sock, buf, sizeof(buf));
                if (debug)
                    printf("read: %s\n", buf);
                /* code */
            } while (len > 0);

            /* 定位本地服务器的html文件*/

            //

            {
                char *pos = strchr(url, '?');
                if (pos)
                {
                    *pos = '\0';
                    printf("real url: %s\n", url);
                }
            }

            // 读取本地文件
            char path[512];
            sprintf(path, "./html_docs/%s", url);
            if (debug)
                printf("path is %s\n", path);

            struct stat st;
            // 读取文件

            if (stat(path, &st) == -1)
            {
                fprintf(stderr, "stat %s failed,reson :%s \n", path, strerror(errno));

                fprintf(stderr, "warning! file not found\n");
                // 返回404
                not_found(client_sock);
            }
            else
            {
                // 文件存在
                if (S_ISDIR(st.st_mode))
                {

                    strcat(path, "/index.html");
                    // 返回目录
                    fprintf(stderr, "warning! file is a dir\n");
                    // 返回404
                }
                do_http_response(client_sock, path);
            }
        }
        else
        {
            fprintf(stderr, "warning! other request[%s]\n", method);
            // 处理其他请求
            if (debug)
                printf("method != GET\n");

            // read http header ,and send error code to the client
            // 响应客户都 501 the method is not defined
            do
            {
                len = get_line(client_sock, buf, sizeof(buf));
                if (debug)
                    printf("read: %s\n", buf);

            } while (len > 0);

            // unimplemented(client_sock);//在响应时实现
        }

        // 读取Http的头部
    }
    else
    {
        // request error
        bad_request(client_sock);
    }
    close(client_sock);
	if(pclient_sock) free(pclient_sock);//释放动态分配的内存
	
	return NULL;
}

int main(void)
{
    int server_sock;
    server_sock = socket(AF_INET, SOCK_STREAM, 0);

    if (server_sock < 0)
    {
        perror("socket");
        // strerror(errno);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (listen(server_sock, SOMAXCONN) < 0)
    {
        perror("listen");
        fprintf(stderr, "errro listen%s \n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("等待客户端的连接\n");
    int done = 1;
    while (done)
    {
        struct sockaddr_in client_addr;
        int client_sock, len, i;
        char client_ip[64];
        char buff[256];

        socklen_t client_addr_len = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);

        printf("client ip:%s\tport:%d\n",
               inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip)),
               ntohs(client_addr.sin_port));
        

        

        // 处理http请求 读取客户端发送的信息
        //使用多线程实现异步访问
        pthread_t tid;
        int * pclient_sock = (int *)malloc(sizeof(int));
		*pclient_sock = client_sock;
        pthread_create(&tid, NULL, do_http_request, (void *)pclient_sock);
        pthread_detach(tid);
         
        if (done == 0)
            break;
        
         
    }

    close(server_sock);
    return 0;
}
