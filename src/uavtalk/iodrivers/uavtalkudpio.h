/**
 ******************************************************************************
 * @file       uavtalkudpio.h
 * @author     Vladimir Ermakov, Copyright (C) 2013.
 * @brief The UAVTalk protocol
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef UAVTALKIOUDP_H
#define UAVTALKIOUDP_H

#include "uavtalkiobase.h"
#include <boost/asio/ip/udp.hpp>
#include <boost/shared_array.hpp>
#include <memory>

namespace openpilot
{

class UAVTalkUDPIO : public UAVTalkIOBase
{
public:
	UAVTalkUDPIO(std::string server_addr, unsigned int server_port);
	~UAVTalkUDPIO();

	void write(const uint8_t *data, size_t length);
	//ssize_t read(uint8_t *data, size_t length);
	//size_t available();
	inline bool is_open() { return socket.is_open(); };

private:
	boost::asio::io_service io_service;
	std::auto_ptr<boost::asio::io_service::work> io_work;
	boost::thread io_thread;
	boost::asio::ip::udp::socket socket;
	boost::asio::ip::udp::endpoint sender_endpoint;

	static const size_t RX_BUFSIZE = 10 + 256 + 1;
	uint8_t rx_buf[RX_BUFSIZE];
	std::vector<uint8_t> tx_q;
	boost::shared_array<uint8_t> tx_buf;
	size_t tx_buf_size;
	boost::recursive_mutex mutex;
	bool sender_exists;

	void do_read(void);
	void async_read_end(boost::system::error_code ec, size_t bytes_transfered);
	void do_write(void);
	void async_write_end(boost::system::error_code ec);
};

} // namespace openpilot

#endif // UAVTALKIOUDP_H

