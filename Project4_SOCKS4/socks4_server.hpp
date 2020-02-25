#ifndef SOCKS4_HPP
#define SOCKS4_HPP

#include <string>
#include <boost/asio.hpp>
#include <boost/array.hpp>

using namespace std;
namespace socks4 {

const unsigned char version = 0x04;

    class request
    {
    private:
        unsigned char version_;
        unsigned char command_;
        unsigned char port_high_byte_;
        unsigned char port_low_byte_;
        boost::asio::ip::address_v4::bytes_type address_;
        string u_id_;
        string domain_name_;
        unsigned char null_byte_;

    public:
        enum command_type
        {
            connect = 0x01,
            bind = 0x02
        };

        request()
            : null_byte_(0)
        {
        }

        void parse_request(string request)
        {
            version_ = request[0];
            command_ = request[1];
            port_high_byte_ = request[2];
            port_low_byte_ = request[3];
            address_[0] = request[4];
            address_[1] = request[5];
            address_[2] = request[6];
            address_[3] = request[7];

            int domain_name_start_idx;
            for(int i=8;i<request.size();i++)
            {
                if(request[i] != '\0')
                {
                    u_id_ += request[i];
                }
                else
                {
                    domain_name_start_idx = i+1;
                    break;
                }
            }

            if(domain_name_start_idx == request.size())
            {
                //do not have domain name: SOCKS4
                return;
            }
            else
            {
                for(int i=domain_name_start_idx;i<request.size();i++)
                {
                    if(request[i] != '\0')
                    {
                        domain_name_ += request[i];
                    }
                    else
                    {
                        break;
                    }                    
                }
            }
            


        }
        /*
        boost::array<boost::asio::mutable_buffer, 7> buffers()
        {
            boost::array<boost::asio::mutable_buffer, 7> bufs =
            {
                {
                    boost::asio::buffer(&version_, 1),
                    boost::asio::buffer(&command_, 1),
                    boost::asio::buffer(&port_high_byte_, 1),
                    boost::asio::buffer(&port_low_byte_, 1),
                    boost::asio::buffer(address_),
                    boost::asio::buffer(u_id),
                    boost::asio::buffer(&null_byte_, 1)
                }
            };
            return bufs;
        }
        */
        boost::asio::ip::tcp::endpoint endpoint() const
        {
            unsigned short port = port_high_byte_;
            port = static_cast<unsigned short>((port << 8) & 0xff00);
            port = port | port_low_byte_;

            boost::asio::ip::address_v4 address(address_);

            return boost::asio::ip::tcp::endpoint(address, port);
        }

        string command() const
        {
            if(command_ == connect)
                return "connect";
            else if(command_ == bind)
                return "bind";
        }

        string operation() const
        {
            if(command_ == connect)
                return "c";
            else if(command_ == bind)
                return "b";
        }

        string print_domain_name() const
        {
            return domain_name_;
        }
        string print_uid() const
        {
            return u_id_;
        }
    };

    class reply
    {
    private:
        unsigned char null_byte_;
        unsigned char status_;
        unsigned char port_high_byte_;
        unsigned char port_low_byte_;
        boost::asio::ip::address_v4::bytes_type address_;

    public:
        enum status_type
        {
            request_granted = 0x5a,
            request_failed = 0x5b,
        };

        reply(status_type status, const boost::asio::ip::tcp::endpoint& endpoint)
                : status_(status),
                null_byte_(0)
        {
            // Only IPv4 is supported by the SOCKS 4 protocol.
            if (endpoint.protocol() != boost::asio::ip::tcp::v4())
            {
                throw boost::system::system_error(
                    boost::asio::error::address_family_not_supported);
            }

            // Convert port number to network byte order.
            unsigned short port = endpoint.port();
            port_high_byte_ = static_cast<unsigned char>((port >> 8) & 0xff);
            port_low_byte_ = static_cast<unsigned char>(port & 0xff);

            // Save IP address in network byte order.
            address_ = endpoint.address().to_v4().to_bytes();
        }
        
        unsigned char status() const { return status_; }


        boost::array<boost::asio::const_buffer, 5> buffers() const
        {
            boost::array<boost::asio::const_buffer, 5> bufs =
            {
                {
                    boost::asio::buffer(&null_byte_, 1),
                    boost::asio::buffer(&status_, 1),
                    boost::asio::buffer(&port_high_byte_, 1),
                    boost::asio::buffer(&port_low_byte_, 1),
                    boost::asio::buffer(address_)
                }
            };
            return bufs;
        }

        

    };

} // namespace socks4

#endif // SOCKS4_HPP