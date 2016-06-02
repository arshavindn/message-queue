#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <unistd.h>  // getpid(), fork()
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>  // pid_t

#define MSGSZ 255

typedef struct msgbuf {
    long    mtype;
    char    mtext[MSGSZ];
} message_buf;

void remove_spaces(char **str)
{
    char *temp;
    temp = *str;
    char c[strlen(*str) + 1];
    int i = 0;
    for( ; *temp != '\0'; ++temp)
        if (!isspace(*temp)) {
            c[i] = *temp;
            i++;
        }
    *str = NULL;
    *str = (char*)malloc(i++*sizeof(char));
    strncpy(*str, c, i*sizeof(char));
    // printf("rs %s\n", *str);
}


char** extract_expression(char* source)
{
    char** result = malloc(3*sizeof(char*));
    char* operator_type;
    operator_type = (char*) malloc(sizeof(char));
    char* pattern = "(\\-?[0-9]+)([\\+-\\*/]{1})(\\-?[0-9]+)";
    size_t maxMatches = 1;
    size_t maxGroups = 4;

    regex_t regexCompiled;
    regmatch_t groupArray[maxGroups];
    unsigned int m;
    char* cursor;

    if (regcomp(&regexCompiled, pattern, REG_EXTENDED))
    {
        printf("Could not compile regular expression.\n");
        return NULL;
    };

    m = 0;
    cursor = source;
    for (m = 0; m < maxMatches; m++)
    {
        if (regexec(&regexCompiled, cursor, maxGroups, groupArray, 0)) {
            printf("Nah\n");
            break;  // No more matches
        }
        // puts("jam");
        unsigned int g = 0;
        unsigned int offset = 0;
        for (g = 0; g < maxGroups; g++)
        {
            // puts("yeah");
            if (groupArray[g].rm_so == (size_t)-1) {
                printf("Yo\n");
                break;  // No more groups
            }

            if (g == 0)
                offset = groupArray[g].rm_eo;

            char cursorCopy[strlen(cursor) + 1];
            strcpy(cursorCopy, cursor);
            cursorCopy[groupArray[g].rm_eo] = 0;
            // printf("Match %u, Group %u: [%2u-%2u]: %s\n",
            //              m, g, groupArray[g].rm_so, groupArray[g].rm_eo,
            //              cursorCopy + groupArray[g].rm_so);
            // printf("%s\n", cursorCopy + groupArray[g].rm_so);
            if (g > 0) {
                result[g-1] = malloc(sizeof(char)*strlen(cursorCopy + groupArray[g].rm_so));
                strcpy(result[g-1], cursorCopy + groupArray[g].rm_so);
            }
        }
        cursor += offset;
    }
    // for (int i = 0; i < 3; i++) puts(result[i]);
    regfree(&regexCompiled);
    free(operator_type);
    return result;
}


float calculate(char** array) {
    if (array != NULL) {
        float x1 = atof(*(array+0));
        float x2 = atof(*(array+2));
        if (strcmp(*(array+1), "+") == 0) return x1 + x2;
        else if (strcmp(*(array+1), "-") == 0) return x1 - x2;
        else if (strcmp(*(array+1), "*") == 0) return x1 * x2;
        else if (strcmp(*(array+1), "/") == 0)
            if (x2 != 0) return x1 / x2;
            else return NAN;
        else return NAN;
    }
    else return NAN;
}


int main() {
    printf("This is main process which has pid is %d\n", getpid());
    /*
     * Get the message queue id for the
     * "name" 1234, which was created by
     * the server.
     */
    key_t KEY = 1234, KEY2 = 5678;
    char FINISHED_MESSAGE[17] = "Sending finished";
    int MSGFLG = IPC_CREAT | 0666;
    pid_t child1_process = fork();
    if (child1_process >= 0) {
        // IN CHILD 1 PROCESS
        if (child1_process == 0) {
            int msqid, msqid2;
            message_buf send_buf, receive_result_buf;
            // We'll send message type 1
            send_buf.mtype = 1;
            size_t buf_length;

            (void) fprintf(stderr, "msgget: Calling msgget(%d,%#o)\n", KEY, MSGFLG);

            if ((msqid = msgget(KEY, MSGFLG)) < 0) {
                perror("msgget");
                exit(1);
            }
            else
                (void) fprintf(stderr,"msgget: msgget succeeded: msqid = %d\n", msqid);

            FILE *fp;
            char buff[255], temp[255];
            (void) strcpy(temp, "");
            fp = fopen("/tmp/test.txt", "r");
            while (1 == 1) {
                fgets(buff, 255, (FILE*)fp);
                if (strcmp(temp, buff) == 0) {
                    fclose(fp);
                    (void) strcpy(send_buf.mtext, FINISHED_MESSAGE);
                }
                else {
                    (void) strcpy(temp, buff);
                    (void) strcpy(send_buf.mtext, buff);
                    if (send_buf.mtext[strlen(send_buf.mtext)-1] == '\n')
                        send_buf.mtext[strlen(send_buf.mtext)-1] = '\0';  // Remove \n character
                }
                // Send a message.
                buf_length = strlen(send_buf.mtext) + 1;
                if (msgsnd(msqid, &send_buf, buf_length, IPC_NOWAIT) < 0) {
                    printf ("%d, %ld, %s, %lu\n", msqid, send_buf.mtype, send_buf.mtext, buf_length);
                    perror("msgsnd");
                    exit(1);
                }
                else {
                    printf("Child 1 Message: \"%s\" Sent\n", send_buf.mtext);
                    if (strcmp(send_buf.mtext, FINISHED_MESSAGE) == 0) break;
                }
            }
            /*
             * RECEIVE RESULT FROM CHILD 2
             */
            if ((msqid2 = msgget(KEY2, 0666)) < 0) {
                perror("msgget");
                exit(1);
            }
            int i = 0;
            fp = fopen("/tmp/test.txt", "w");
            while (1 == 1) {
                if (msgrcv(msqid2, &receive_result_buf, MSGSZ, 1, 0) < 0) {
                    exit(1);
                }
                if (strcmp(receive_result_buf.mtext, FINISHED_MESSAGE) == 0) break;
                printf("Result %d: %s\n", i, receive_result_buf.mtext);
                if (i == 0) {
                    fputs(receive_result_buf.mtext, fp);
                    fputs("\n", fp);
                    fclose(fp);
                    fp = fopen("/tmp/test.txt", "a");
                }
                else {
                    fputs(receive_result_buf.mtext, fp);
                    fputs("\n", fp);
                }
                i++;
            }
            fclose(fp);
        }
        // IN MAIN PROCESS
        else {
            pid_t child2_process = fork();
            if (child2_process >= 0) {
                // IN CHILD 2 PROCESS
                if (child2_process == 0) {
                    int msqid, msqid2;
                    size_t buf_length;
                    message_buf  receive_buf, send_back_buf;
                    send_back_buf.mtype = 1;
                    char *source;
                    char result[30];

                    printf("Child 2 pid = %d\n", getpid());

                    if ((msqid = msgget(KEY, 0666)) < 0) {
                        perror("msgget");
                        exit(1);
                    }
                    // queue for send back the result
                    (void) fprintf(stderr, "msgget: Calling msgget(%d,%#o)\n", KEY2, MSGFLG);

                    if ((msqid2 = msgget(KEY2, MSGFLG)) < 0) {
                        perror("msgget");
                        exit(1);
                    }
                    else
                        (void) fprintf(stderr,"msgget: msgget succeeded: msqid = %d\n", msqid2);

                    while (1 == 1) {
                        // Receive an answer of message type 1.
                        if (msgrcv(msqid, &receive_buf, MSGSZ, 1, 0) < 0) {
                            perror("msgrcv");
                            exit(1);
                        }

                        if (strcmp(receive_buf.mtext, FINISHED_MESSAGE) == 0) strcpy(result, FINISHED_MESSAGE);
                        else {
                            source = (char*)malloc(strlen(receive_buf.mtext)*sizeof(char));  // remember free source
                            strcpy(source, receive_buf.mtext);
                            remove_spaces(&source);
                            // Copy result from calculate func to result variable with format.
                            sprintf(result, "%-5.2f", calculate(extract_expression(source)));
                            // Join source string with result and assign to result variable.
                            char temp[strlen(source)+strlen("=")+strlen(result)+1];
                            strcpy(temp, source);
                            strcat(temp, "=");
                            strcat(temp, result);
                            strcpy(result, temp);
                        }
                        /*
                         * PREPAIRE QUEUE FOR SENDING BACK TO CHILD1
                         */
                        (void) strcpy(send_back_buf.mtext, result);
                        buf_length = strlen(send_back_buf.mtext) + 1;
                        if (msgsnd(msqid2, &send_back_buf, buf_length, IPC_NOWAIT) < 0) {
                            printf ("%d, %ld, %s, %lu\n", msqid2, send_back_buf.mtype, send_back_buf.mtext, buf_length);
                            perror("msgsnd");
                            exit(1);
                        }
                        else {
                            printf("Child 2 Result: \"%s\" Sent\n", send_back_buf.mtext);
                            if (strcmp(send_back_buf.mtext, FINISHED_MESSAGE) == 0) break;
                        }
                        // free source
                        free(source);
                    }
                }
                else {
                    // wait for all child finish
                    sleep(1);
                }
            }
            else {
                puts("Error occured in forking.");
            }
        }
    }
    else {
        printf("Error occured in forking.\n");
    }
}
