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
namespace otdriver{
int gen_stat(std::vector<float> fres, const char* path, int block_size){
    std::vector<float> buf(block_size*2);
    int n=fres.size();
    for (int i = 0; i < block_size; i++)
    {
        for (auto f : fres)
        {
            buf[2*i]+=cos(f*PI_10*i)/n;
            buf[2*i+1]+=sin(f*PI_10*i)/n;
        }
        
    }
    int fd=open(path,O_CREAT|O_WRONLY,S_IRWXU);
    if(fd<0){
        std::cerr<<"open "<<path<<" failed\n";
        return -1;
    }
    if(write(fd,buf.data(),sizeof(float)*buf.size())!=(ssize_t)(sizeof(float)*buf.size())){
        close(fd);
        std::cerr<<"writing data dailed\n";
        return -1;
    }
    close(fd);
    fd=open(path,O_RDONLY);
    if(fd<0){
        std::cerr<<"open "<<path<<" failed\n";
        return -1;
    }
    return fd;
}

int gen_move(std::vector<float> old_fres, std::vector<float> des_fres, const char* path, int step) //step->1ms
{
    if(old_fres.size()!=des_fres.size()){
        fprintf(stderr, "old freq and des freq have different size!\n");
        return -1;
    }
    std::vector<float> buf(step*2);
    int n=old_fres.size();
    for (auto &&f : des_fres){
        f/=step;
    }
    for (int i = 0; i < step; i++){
        for (auto f=old_fres.begin(), df=des_fres.begin(), fe=old_fres.end(); f!=fe; ++f, ++df ){
            (*f)+=(*df);
            buf[2*i]+=cos((*f)*PI_10*i)/n;
            buf[2*i+1]+=sin((*f)*PI_10*i)/n;
        }
    }
    int fd=open(path,O_CREAT|O_WRONLY,S_IRWXU);
    if(fd<0){
        std::cerr<<"open "<<path<<" failed\n";
        return -1;
    }
    if(write(fd,buf.data(),sizeof(float)*buf.size())!=(ssize_t)(sizeof(float)*buf.size())){
        close(fd);
        std::cerr<<"writing data dailed\n";
        return -1;
    }
    close(fd);
    fd=open(path,O_RDONLY);
    if(fd<0){
        std::cerr<<"open "<<path<<" failed\n";
        return -1;
    }
    return fd;
}

int init(std::vector<float>& fres){
    std::cout<<"generating initial file"<<std::endl;
    bool pass_flag;
    do{
        int n_fres;
        std::cout<<"number of frequencies:";
        std::cin>>n_fres;
        std::cout<<"input your frequency, unit: MHz\n";
        float f;
        for (int i = 0; i < n_fres; i++){
            std::cin>>f;
            fres.push_back(f);
        }
        std::sort(fres.begin(),fres.end());
        std::cout<<"frequencies:";
        for (auto &&f : fres){
            std::cout<<f<<",";
        }
        std::cout<<std::endl<<"countinue? 1 for true, 0 for false. ";
        std::cin>>pass_flag;
    }while (!pass_flag);
    int fd=gen_stat(fres,"./init.dat");
    if(fd<0){
        std::cerr<<"generate init file error, quiting.\n";
        return -1;
    }
    return fd;
}

void run(file_descriptor_source_impl* s){
    std::vector<float> fres(s->get_fres());
    std::cout<<"start listen loop"<<std::endl;
    std::string help_string("command args\nquit\nhelp\nlist\nsort\nmove index move_to\nmoveall f1 f2 ...\ndelete index\nadd new_freq\n");
    int stat_file_count=0;
    while (true)
    {
        std::string cmd;
        std::cin>>cmd;
        if(cmd=="quit"){
            break;
        }
        else if (cmd=="help"){
            std::cout<<help_string;
        }
        else if (cmd=="list"){
            std::cout<<"current frequencies\n";
            for (size_t i = 0; i < fres.size(); i++)
            {
                std::cout<<"f"<<i<<"="<<fres[i]<<"\t";
            }
            std::cout<<std::endl;
        }
        else if (cmd=="sort"){
            std::sort(fres.begin(),fres.end());
            std::cout<<"current frequencies\n";
            for (size_t i = 0; i < fres.size(); i++){
                std::cout<<"f"<<i<<"="<<fres[i]<<"\t";
            }
            std::cout<<std::endl;
        }
        else if (cmd=="move"){
            auto old_fres=fres;
            int n;
            float des;
            std::cin>>n>>des;
            fres[n]=des;
            int fd_m,fd_s;
            fd_m=gen_move(old_fres,fres,"./move.dat");
            std::stringstream stat_name;
            stat_name<<"./stat"<<(++stat_file_count)<<".dat";
            fd_s=gen_stat(fres,stat_name.str().c_str());
            if(fd_m>2){
                fprintf(stderr,"move file added, fd=%d\n",fd_m);
                s->add_fd(fd_m);
            }else{
                fprintf(stderr,"can't open move file, fd=%d\n",fd_m);
            }
            if(fd_s>2){
                fprintf(stderr,"stat file added, fd=%d\n",fd_s);
                s->add_fd(fd_s);
            }else{
                fprintf(stderr,"can't open stat file, fd=%d\n",fd_s);
            }
        }
        else if (cmd=="moveall"){
            auto old_fres=fres;
            int n=fres.size();
            float des;
            for (int i = 0; i < n; i++)
            {
                std::cin>>des;
                fres[i]=des;
            }
            int fd_m,fd_s;
            fd_m=gen_move(old_fres,fres,"./move.dat");
            std::stringstream stat_name;
            stat_name<<"./stat"<<(++stat_file_count)<<".dat";
            fd_s=gen_stat(fres,stat_name.str().c_str());
            if(fd_m>2){
                fprintf(stderr,"move file added, fd=%d\n",fd_m);
                s->add_fd(fd_m);
            }else{
                fprintf(stderr,"can't open move file, fd=%d\n",fd_m);
            }
            if(fd_s>2){
                fprintf(stderr,"stat file added, fd=%d\n",fd_s);
                s->add_fd(fd_s);
            }else{
                fprintf(stderr,"can't open stat file, fd=%d\n",fd_s);
            }
        }
        else if (cmd=="delete"){
            int n;
            std::cin>>n;
            if(n<0||n>=(int)(fres.size())){
                std::cerr<<"n out of range\n";
                continue;
            }
            fres.erase(fres.begin()+n);
            int fd_s;
            std::stringstream stat_name;
            stat_name<<"./stat"<<(++stat_file_count)<<".dat";
            fd_s=gen_stat(fres,stat_name.str().c_str());
            if(fd_s>2){
                fprintf(stderr,"stat file added, fd=%d\n",fd_s);
                s->add_fd(fd_s);
            }else{
                fprintf(stderr,"can't open stat file, fd=%d\n",fd_s);
            }
        }
        else if (cmd=="add")
        {
            float f;
            std::cin>>f;
            fres.push_back(f);
            int fd_s;
            std::stringstream stat_name;
            stat_name<<"./stat"<<(++stat_file_count)<<".dat";
            fd_s=gen_stat(fres,stat_name.str().c_str());
            if(fd_s>2){
                fprintf(stderr,"stat file added, fd=%d\n",fd_s);
                s->add_fd(fd_s);
            }else{
                fprintf(stderr,"can't open stat file, fd=%d\n",fd_s);
            }
        }
        else{
            std::cout<<"unknown command\n";
        }
        std::cin.ignore(__INT_MAX__,'\n');
    }
}
}// namespace otdriver

void file_descriptor_source_impl::add_fd(const int fd){
    fd_queue.push(fd);
}
std::vector<float> file_descriptor_source_impl::get_fres(){
    return fres;
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
      d_fd(otdriver::init(fres)),
      d_repeat(repeat),
      d_residue(itemsize),
      d_residue_len(0),
      listen_thread(otdriver::run,this)
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
                //system("date +%T.%N");
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
