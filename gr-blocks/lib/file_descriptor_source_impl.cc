/* -*- c++ -*- */
/*
 * Copyright 2004,2005,2013 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "file_descriptor_source_impl.h"
#include <gnuradio/io_signature.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <boost/format.hpp>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#ifdef HAVE_IO_H
#include <io.h>
#endif

namespace gr {
namespace blocks {

void file_descriptor_source_listen(file_descriptor_source_impl* s){
    std::string file_name1;
    std::string file_name2;
    int fd1;
    int fd2;
    while(true){
        std::cin>>file_name1>>file_name2;
        system("date +%T.%N");
        if(file_name1=="quit"||file_name2=="quit"){
            break;
        }
        fd1=open(file_name1.c_str(),O_RDONLY);
        fd1=open(file_name1.c_str(),O_RDONLY);
        if(fd1>2&&fd2>2){
            fprintf(stderr,"file added: %s, fd=%d\n",file_name1.c_str(),fd1);
            fprintf(stderr,"file added: %s, fd=%d\n",file_name2.c_str(),fd2);
            s->add_fd(fd1);
            s->add_fd(fd2);
        }else{
            fprintf(stderr,"can't open file: %s, fd=%d\n",file_name1.c_str(),fd1);
            fprintf(stderr,"can't open file: %s, fd=%d\n",file_name2.c_str(),fd2);
        }
    }
}

void file_descriptor_source_impl::add_fd(const int fd){
    fd_queue.push(fd);
}

file_descriptor_source::sptr
file_descriptor_source::make(size_t itemsize, int fd, bool repeat)
{
    return gnuradio::make_block_sptr<file_descriptor_source_impl>(itemsize, fd, repeat);
}

file_descriptor_source_impl::file_descriptor_source_impl(size_t itemsize,
                                                         int fd,
                                                         bool repeat)
    : sync_block("file_descriptor_source",
                 io_signature::make(0, 0, 0),
                 io_signature::make(1, 1, itemsize)),
      d_itemsize(itemsize),
      d_fd(fd),
      d_repeat(repeat),
      d_residue(itemsize),
      d_residue_len(0),
      listen_thread(file_descriptor_source_listen,this)
{
}

file_descriptor_source_impl::~file_descriptor_source_impl() {
    close(d_fd);
    listen_thread.join();
}

int file_descriptor_source_impl::read_items(char* buf, int nitems)
{
    assert(nitems > 0);
    assert(d_residue_len < d_itemsize);

    int nbytes_read = 0;

    if (d_residue_len > 0) {
        memcpy(buf, d_residue.data(), d_residue_len);
        nbytes_read = d_residue_len;
        d_residue_len = 0;
    }

    int r = read(d_fd, buf + nbytes_read, nitems * d_itemsize - nbytes_read);
    if (r <= 0) {
        handle_residue(buf, nbytes_read);
        return r;
    }

    r = handle_residue(buf, r + nbytes_read);

    if (r == 0) // block until we get something
        return read_items(buf, nitems);

    return r;
}

int file_descriptor_source_impl::handle_residue(char* buf, int nbytes_read)
{
    assert(nbytes_read >= 0);
    const unsigned int nitems_read = nbytes_read / d_itemsize;
    d_residue_len = nbytes_read % d_itemsize;
    if (d_residue_len > 0) {
        // fprintf (stderr, "handle_residue: %d\n", d_residue_len);
        memcpy(d_residue.data(), buf + nbytes_read - d_residue_len, d_residue_len);
    }
    return nitems_read;
}

int file_descriptor_source_impl::work(int noutput_items,
                                      gr_vector_const_void_star& input_items,
                                      gr_vector_void_star& output_items)
{
    assert(noutput_items > 0);

    char* o = (char*)output_items[0];
    int nread = 0;

    while (1) {
        int r = read_items(o, noutput_items - nread);
        if (r == -1) {
            if (errno == EINTR)
                continue;
            else {
                GR_LOG_ERROR(d_logger,
                             boost::format("file_descriptor_source[read]: %s") %
                                 strerror(errno));
                return -1;
            }
        } else if (r == 0) { // end of file
            if (!fd_queue.empty()){
                fprintf(stderr,"closing fd:%d,",d_fd);
                close(d_fd);
                d_fd=fd_queue.front();
                fprintf(stderr,"changing fd:%d\n",d_fd);
                fd_queue.pop();
                system("date +%T.%N");
            }
            else {
                flush_residue();
                if (lseek(d_fd, 0, SEEK_SET) == -1) {
                    GR_LOG_ERROR(d_logger,
                                 boost::format("file_descriptor_source[lseek]: %s") %
                                     strerror(errno));
                    return -1;
                }
            }
        } else {
            o += r * d_itemsize;
            nread += r;
            break;
        }
    }

    if (nread == 0) // EOF
        return -1;

    return nread;
}

} /* namespace blocks */
} /* namespace gr */
