/**
 *  Serialization / deserialization consistency test
 *  This example reads raw binson .bson file, parse it
 *  to DOM model, output DOM model to console as HEX + JSON
 *  and serialize back to another file to make sure both
 *  files are identical
 */

#include <stdio.h>
#include "binson/binson.h"
#include "common.h"

int main()
{
    binson          *context;
    binson_writer   *writer;
    binson_parser   *parser;

    binson_io       *err_io, *fin, *fout, *con;
    binson_res       res;

    /* DEBUG: disable stdout buffering for debugging purposes */
    /*setvbuf(stdout, NULL, _IONBF, 0);*/

    res = binson_io_new( &err_io );
    res = binson_io_new( &fin );
    res = binson_io_new( &fout );
    res = binson_io_new( &con );

    res = binson_io_attach_stream( err_io, stdout );
    res = binson_io_attach_stream( con, stdout );
    res = binson_io_open_file( fin, "./output_model_dom.bson", BINSON_IO_MODE_READ );
    res = binson_io_open_file( fout, "./recycle.bson", BINSON_IO_MODE_WRITE | BINSON_IO_MODE_CREATE );

    res = binson_parser_new( &parser );
    res = binson_parser_init( parser, fin, BINSON_PARSER_MODE_DOM );

    res = binson_writer_new( &writer );
    res = binson_writer_init( writer, con, BINSON_WRITER_FORMAT_JSON_NICE );

    res = binson_new( &context );
    res = binson_init( context, err_io );

    /* ready to build DOM */
    res = binson_deserialize( context, parser, NULL, "def_key", false ); /* NULL mean replace whole DOM tree */

    /* serialize via attached 'binson_writer' */
    res = binson_serialize( context, writer, NULL );
    res = binson_io_printf( con, "\n---------------\n" );
    res = binson_writer_set_format( writer, BINSON_WRITER_FORMAT_HEX );
    res = binson_serialize( context, writer, NULL );
    res = binson_io_printf( con, "\n---------------\n" );

    /* save to file as raw binson format */
    res = binson_writer_set_format( writer, BINSON_WRITER_FORMAT_RAW );
    res = binson_writer_set_io( writer, fout );
    res = binson_serialize( context, writer, NULL );

    /* we are done. freeing resources */
    res = binson_free( context );
    res = binson_parser_free( parser );
    res = binson_writer_free( writer );

    res = binson_io_free( err_io );
    res = binson_io_free( fin );
    res = binson_io_free( fout );
    res = binson_io_free( con );

   return res;
}
