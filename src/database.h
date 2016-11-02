#ifndef DATABASE_H
#define DATABASE_H

void	close_database();
void	init_database();
int	process_get_query(char *msg, char *newmsg);
int	process_post_query(char *msg, char *newmsg);

#endif
