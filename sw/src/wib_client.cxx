#include <unistd.h>
#include <fstream>
#include <streambuf>

#include <zmq.hpp>
#include <readline/readline.h>
#include <readline/history.h>

#include "wib.pb.h"

using namespace std;

template <class R, class C>
void send_command(zmq::socket_t &socket, const C &msg, R &repl) {

    wib::Command command;
    command.mutable_cmd()->PackFrom(msg);
    
    string cmd_str;
    command.SerializeToString(&cmd_str);
    
    zmq::message_t request(cmd_str.size());
    memcpy((void*)request.data(), cmd_str.c_str(), cmd_str.size());
    socket.send(request,zmq::send_flags::none);
    
    zmq::message_t reply;
    socket.recv(reply,zmq::recv_flags::none);
    
    string reply_str(static_cast<char*>(reply.data()), reply.size());
    repl.ParseFromString(reply_str);
    
}

int run_command(zmq::socket_t &s, int argc, char **argv) {
    if (argc < 1) return 1;
    
    string cmd(argv[0]);
    if (cmd == "exit") { 
        return 255;
    } else if (cmd == "get_sensors") {
        wib::GetSensors req;
        wib::Sensors rep;
        send_command(s,req,rep);
    } else if (cmd == "update") {
        if (argc != 3) {
            fprintf(stderr,"Usage: update root_archive boot_archive\n");
            return 0;
        }
        
        size_t length;
        string root_archive, boot_archive;
        
        ifstream in_root(argv[1], ios::binary);
        in_root.ignore( numeric_limits<streamsize>::max() );
        length = in_root.gcount();
        in_root.clear();
        in_root.seekg( 0, ios_base::beg );
        root_archive.resize(length);
        in_root.read((char*)root_archive.data(),length);
        
        ifstream in_boot(argv[2], ios::binary);
        in_boot.ignore( numeric_limits<streamsize>::max() );
        length = in_boot.gcount();
        in_boot.clear();
        in_boot.seekg( 0, ios_base::beg );
        boot_archive.resize(length);
        in_boot.read((char*)boot_archive.data(),length);
        
        wib::Update req;
        printf("Sending root archive (%0.1f MB) and boot archive (%0.1f MB)\n",root_archive.size()/1024.0/1024.0,boot_archive.size()/1024.0/1024.0);
        req.set_root_archive(root_archive);
        req.set_boot_archive(boot_archive);
        wib::Empty rep;
        send_command(s,req,rep);
    } else if (cmd == "initialize") {
        wib::Initialize req;
        wib::Empty rep;
        send_command(s,req,rep);
    } else if (cmd == "reboot") {
        wib::Reboot req;
        wib::Empty rep;
        send_command(s,req,rep);
    } else if (cmd == "peek") {
        if (argc != 2) {
            fprintf(stderr,"Usage: peek addr\n");
            fprintf(stderr,"\t all arguments are base 16\n");
            return 0;
        }
        wib::Peek req;
        req.set_addr((size_t)strtoull(argv[1],NULL,16));
        wib::RegValue rep;
        send_command(s,req,rep);
        printf("0x%X\n",rep.value());
    } else if (cmd == "poke") {
        if (argc != 3) {
            fprintf(stderr,"Usage: poke addr value\n");
            fprintf(stderr,"\t all arguments are base 16\n");
            return 0;
        }
        wib::Poke req;
        req.set_addr((size_t)strtoull(argv[1],NULL,16));
        req.set_value((uint32_t)strtoull(argv[2],NULL,16));
        wib::RegValue rep;
        send_command(s,req,rep);
    } else if (cmd == "cdpeek") {
        if (argc != 6) {
            fprintf(stderr,"Usage: cdpeek femb_idx cd_idx chip_addr reg_page reg_addr\n");
            fprintf(stderr,"\t femb_idx and cd_idx are base 10, remaining args are base 16\n");
            return 0;
        }
        wib::CDPeek req;
        req.set_femb_idx((uint8_t)strtoull(argv[1],NULL,10));
        req.set_coldata_idx((uint8_t)strtoull(argv[2],NULL,10));
        req.set_chip_addr((uint8_t)strtoull(argv[3],NULL,16));
        req.set_reg_page((uint8_t)strtoull(argv[4],NULL,16));
        req.set_reg_addr((uint8_t)strtoull(argv[5],NULL,16));
        wib::CDRegValue rep;
        send_command(s,req,rep);
        printf("0x%X\n",rep.data());
    } else if (cmd == "cdpoke") {
        if (argc != 7) {
            fprintf(stderr,"Usage: cdpoke femb_idx cd_idx chip_addr reg_page reg_addr data\n");
            fprintf(stderr,"\t femb_idx and cd_idx are base 10, remaining args are base 16\n");
            return 0;
        }
        wib::CDPoke req;
        req.set_femb_idx((uint8_t)strtoull(argv[1],NULL,10));
        req.set_coldata_idx((uint8_t)strtoull(argv[2],NULL,10));
        req.set_chip_addr((uint8_t)strtoull(argv[3],NULL,16));
        req.set_reg_page((uint8_t)strtoull(argv[4],NULL,16));
        req.set_reg_addr((uint8_t)strtoull(argv[5],NULL,16));
        req.set_data((uint8_t)strtoull(argv[6],NULL,16));
        wib::CDRegValue rep;
        send_command(s,req,rep);
    } else if (cmd == "help") {
        printf("Available commands:\n");
        printf("\treboot\n");
        printf("\t\tReboot the WIB\n");
        printf("\tinitialize\n");
        printf("\t\tInitialize the WIB hardware\n");
        printf("\tpeek addr\n");
        printf("\t\tRead a 32bit value from WIB address space\n");
        printf("\tpoke addr value\n");
        printf("\t\tWrite a 32bit value to WIB address space\n");
        printf("\tcdpeek femb_idx cd_idx chip_addr reg_page reg_addr\n");
        printf("\t\tRead a 8bit value from COLDATA I2C address space\n");
        printf("\tcdpoke femb_idx cd_idx chip_addr reg_page reg_addr data\n");
        printf("\t\tWrite a 8bit value to COLDATA I2C address space\n");
        printf("\tupdate root_archive boot_archive\n");
        printf("\t\tDeploy a new root and boot archive to the WIB\n");
    } else {
        fprintf(stderr,"Unrecognized Command: %s\n",argv[0]);
        return 0;
    }
    return 0;
}

int main(int argc, char **argv) {
    
    char *ip = (char*)"127.0.0.1";
    
    signed char opt;
    while ((opt = getopt(argc, argv, "w:")) != -1) {
       switch (opt) {
           case 'w':
               ip = optarg;
               break;
           default: /* '?' */
               fprintf(stderr, "Usage: %s [-w ip] [cmd] \n", argv[0]);
               return 1;
       }
    }
    
    
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REQ);
    
    char *addr;
    int len = asprintf(&addr,"tcp://%s:1234",ip);
    if (len < 0) return 1;
    
    socket.connect(addr);
    free(addr);
    
    if (optind < argc) {
        return run_command(socket,argc-optind,argv+optind);
    } else {
        char* buf;
        while ((buf = readline(">> ")) != nullptr) {
            if (strlen(buf) > 0) {
                add_history(buf);
            } else {
                free(buf);
                continue;
            }
            char *delim = (char*)" ";   
            int count = 1;
            char *ptr = buf;
            while((ptr = strchr(ptr, delim[0])) != NULL) {
                count++;
                ptr++;
            }
            if (count > 0) {
                char **cmd = new char*[count];
                cmd[0] = strtok(buf, delim);
                int i;
                for (i = 1; cmd[i-1] != NULL && i < count; i++) {
                    cmd[i] = strtok(NULL, delim);
                }
                int ret = run_command(socket,i,cmd);
                delete [] cmd;
                if (ret == 255) return 0;
                if (ret != 0) return ret;
            } else {
                return 0;
            }
            free(buf);
        }
    }
    
    return 0;
}
