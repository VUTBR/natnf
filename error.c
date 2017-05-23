/***********************************************/
/*          Jakub Mackovič - xmacko00          */
/*          Jakub Pastuszek - xpastu00         */
/*               VUT FIT Brno                  */
/*      Export informací o překladu adres      */
/*               Květen 2017                   */
/*                error.c                      */
/***********************************************/

void error(char *msg)
{
    perror(msg);
    exit(1);
}
