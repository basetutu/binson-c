/**
 * Test used with `binson_fuzzer` Java stress tool 
 */
#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include "binson/binson.h"


/***********************************************/
static void sig_handler(int sig, siginfo_t *si, void *unused)
{
    (void)unused;
    printf("Got signal with code %d at address: 0x%lx\n", sig, (long) si->si_addr);
    exit(sig);
}

#define handle_error(msg, ecode) \
    do { perror(msg); exit(ecode); } while (0)
      
/***********************************************/
int main( int argc, char** argv )
{
    FILE    *infile = 0;
    uint8_t  *sbuf = 0;
    uint8_t  *dbuf;
    
    size_t     numbytes, sread;
  
    binson          *obj;
    binson_writer   *writer;
    binson_parser   *parser;

    binson_io       *io_err, *io_in, *io_out;
    binson_res       res;
    binson_raw_size  rs = 0;
    
    int 	     cmp_res = 0;
    size_t	     i = 0;
    struct sigaction sa;
    
    UNUSED(res);
    UNUSED(argc);     
  
    /*------------------------*/
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = sig_handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1) handle_error("sigaction", SIGSEGV);    
    if (sigaction(SIGABRT, &sa, NULL) == -1) handle_error("sigaction", SIGABRT);    
    if (sigaction(SIGFPE, &sa, NULL) == -1) handle_error("sigaction", SIGFPE);    
    if (sigaction(SIGHUP, &sa, NULL) == -1) handle_error("sigaction", SIGHUP);    
    if (sigaction(SIGILL, &sa, NULL) == -1) handle_error("sigaction", SIGILL);    
    if (sigaction(SIGINT, &sa, NULL) == -1) handle_error("sigaction", SIGINT);    
    if (sigaction(SIGPIPE, &sa, NULL) == -1) handle_error("sigaction", SIGPIPE);    
    if (sigaction(SIGTERM, &sa, NULL) == -1) handle_error("sigaction", SIGTERM);    
    /*------------------------*/
    
    
    if (!argv[1] || !strlen(argv[1]))
      return 255;    
    
    infile = fopen(argv[1], "r");
    if(infile == NULL)
      return 2;
    
    fseek(infile, 0, SEEK_END);
    numbytes = (size_t)ftell(infile);
    fseek(infile, 0, SEEK_SET);	

    sbuf = (uint8_t*)calloc(numbytes, sizeof(uint8_t));	
    dbuf = (uint8_t*)calloc(numbytes*2, sizeof(uint8_t));	
  
    if (!sbuf || !dbuf)
      return 3;
  
    sread = fread(sbuf, sizeof(uint8_t), numbytes, infile);
    fclose(infile);
  
    if (sread != numbytes)
      return 4;
    
    res = binson_io_new( &io_in );
    res = binson_io_new( &io_out );
    res = binson_io_new( &io_err );

    res = binson_io_attach_stream( io_err, stdout );
    res = binson_io_attach_bytebuf( io_in, sbuf, numbytes );
    res = binson_io_attach_bytebuf( io_out, dbuf, numbytes*2 );
    
    res = binson_parser_new( &parser );
    res = binson_parser_init( parser, io_in, BINSON_PARSER_MODE_DOM );

    res = binson_writer_new( &writer );
    res = binson_writer_init( writer, io_out, BINSON_WRITER_FORMAT_RAW );

    res = binson_new( &obj );
    res = binson_init( obj, io_err );    
    
    
    res = binson_deserialize( obj, parser, NULL, NULL, false ); 
    res = binson_serialize( obj, writer, &rs );  		      

    if (rs < numbytes)
      return 199;
    
    
    cmp_res = 0;
    for (i=0; i<numbytes; i++)
    {
      if (sbuf[i] != dbuf[i])
      {
	cmp_res = 200;
	break;
      }
    }
            
    res = binson_writer_free( writer );
    res = binson_parser_free( parser );
    res = binson_io_free( io_in );
    res = binson_io_free( io_out );
    res = binson_io_free( io_err );        
    res = binson_free( obj );

    
    free(sbuf);
    free(dbuf);
        
    
   return cmp_res;
}

