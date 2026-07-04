// LLM GENERATED CODE STARTS HERE
#ifndef CLIENT_COMMANDS_H
#define CLIENT_COMMANDS_H

// Execute a command
int execute_command(const char *command, const char *username);
void process_client_command(int ns_sock, char *cmd);

#endif
// LLM GENERATED CODE ENDS HERE