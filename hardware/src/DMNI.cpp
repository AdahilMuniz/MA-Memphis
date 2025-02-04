/**
 * MA-Memphis
 * @file DMNI.hpp
 * 
 * @author Unknown
 * GAPH - Hardware Design Support Group (https://corfu.pucrs.br/)
 * PUCRS - Pontifical Catholic University of Rio Grande do Sul (http://pucrs.br/)
 * 
 * @date September 2013
 * 
 * @brief Implements a DMA and NI module.
 */

#include <cstdlib>

#include "DMNI.hpp"

DMNI::DMNI(sc_module_name name_, regmetadeflit address_router_, std::string path_) :
	sc_module(name_), 
	address_router(address_router_),
	path(path_)
{

	SC_METHOD(arbiter);
	sensitive << clock.pos();
	sensitive << reset;

	SC_METHOD(config);
	sensitive << clock.pos();

	SC_METHOD(receive);
	sensitive << clock.pos();
	sensitive << reset;

	SC_METHOD(send);
	sensitive << clock.pos();
	sensitive << reset;

	SC_METHOD(buffer_control);
	sensitive << add_buffer;
	sensitive << first;
	sensitive << last;

	SC_METHOD(credit_o_update);
	sensitive << slot_available;

	SC_METHOD(mem_address_update);
	sensitive << recv_address;
	sensitive << send_address;
	sensitive << br_mem_addr;
	sensitive << br_mem_data;
	sensitive << br_byte_we;
	sensitive << noc_byte_we;
	sensitive << noc_data_write;
	sensitive << br_payload;
	sensitive << ARB;
	sensitive << last_arb;

	SC_METHOD(br_receive);
	sensitive << clock.pos();
	sensitive << reset;
}

void DMNI::arbiter()
{
	if(reset.read() == 1){
		write_enable = false;
		read_enable = false;
		br_rcv_enable = false;
		last_arb = SEND;
		timer = 0;

		ARB = ROUND;
		return;
	}

	switch(ARB){
		case ROUND:
		{
			switch(last_arb){
				case SEND:
				{
					if(DMNI_Receive == COPY_TO_MEM){
						ARB = RECEIVE;
						write_enable = true;
					} else if(br_req_mon && !br_ack_mon && monitor_ptrs[br_mon_svc] != 0){
						ARB = BR_RECEIVE;
						br_rcv_enable = true;
					} else if(send_active){
						ARB = SEND;
						read_enable = true;
					}
					break;
				}
				case RECEIVE:
				{
					if(br_req_mon && !br_ack_mon && monitor_ptrs[br_mon_svc] != 0){
						ARB = BR_RECEIVE;
						br_rcv_enable = true;
					} else if(send_active){
						ARB = SEND;
						read_enable = true;
					} else if(DMNI_Receive == COPY_TO_MEM){
						ARB = RECEIVE;
						write_enable = true;
					}
					break;
				}
				case BR_RECEIVE:
				{
					if(send_active){
						ARB = SEND;
						read_enable = true;
					} else if(DMNI_Receive == COPY_TO_MEM){
						ARB = RECEIVE;
						write_enable = true;
					} else if(br_req_mon && !br_ack_mon && monitor_ptrs[br_mon_svc] != 0){
						ARB = BR_RECEIVE;
						br_rcv_enable = true;
					}
					break;
				}
				default:
					break;
			}
			break;
		}
		case SEND:
		{
			if(DMNI_Send == END || (timer >= DMNI_TIMER && receive_active)){
				timer = 0;
				ARB = ROUND;
				last_arb = SEND;
				read_enable = false;
			} else {
				timer = timer + 1;
			}
			break;
		}
		case RECEIVE:
		{
			if(DMNI_Receive == END || (timer >= DMNI_TIMER && send_active)){
				timer = 0;
				ARB = ROUND;
				last_arb = RECEIVE;
				write_enable = false;
			} else {
				timer = timer + 1;
			}
			break;
		}
		case BR_RECEIVE:
		{
			if(br_rcv_state == BR_RCV_END){
				ARB = ROUND;
				last_arb = BR_RECEIVE;
				br_rcv_enable = false;
			}
			break;
		}
	}
}

void DMNI::config()
{
	if(reset){
		for(int i = 0; i < MON_TABLE_MAX; i++)
			monitor_ptrs[i] = 0;

		return;
	}

	if(set_address){
		address = config_data;
		address_2 = 0;
		// if(address_router == 1)
		// 	cout << "[DMNI HW] pointer = " << std::hex << config_data << std::endl;
	} else if(set_address_2){
		address_2 = config_data;
	} else if(set_size){
		size = config_data;
		size_2 = 0;
	} else if(set_size_2){
		size_2 = config_data;
	} else if(set_op){
		operation = config_data.read();
	} else if(set_mon_qos){
		monitor_ptrs[MON_TABLE_QOS] = config_data.read();
	} else if(set_mon_pwr){
		monitor_ptrs[MON_TABLE_PWR] = config_data.read();
	} else if(set_mon_2){
		monitor_ptrs[MON_TABLE_2] = config_data.read();
	} else if(set_mon_3){
		monitor_ptrs[MON_TABLE_3] = config_data.read();
	} else if(set_mon_4){
		monitor_ptrs[MON_TABLE_4] = config_data.read();
	}
}

void DMNI::mem_address_update()
{
	if(ARB == SEND || (ARB == ROUND && last_arb == SEND)){
		mem_address.write(send_address.read());
		mem_byte_we.write(0);
	} else if(ARB == RECEIVE || (ARB == ROUND && last_arb == RECEIVE)){
		mem_address.write(recv_address.read());
		mem_data_write.write(noc_data_write.read());
		mem_byte_we.write(noc_byte_we.read());
	} else if(ARB == BR_RECEIVE || (ARB == ROUND && last_arb == BR_RECEIVE)){
		mem_address.write(br_mem_addr.read());
		mem_data_write.write(br_mem_data.read());
		mem_byte_we.write(br_byte_we.read());
	}
}

void DMNI::credit_o_update() {
	credit_o.write(slot_available.read());
}

void DMNI::buffer_control(){

	//Buffer full
	if ( ( first.read() == last.read() ) && add_buffer.read() == 1){
		slot_available.write(0);
	} else {
		slot_available.write(1);
	}

	//Buffer empty
	if ( ( first.read() == last.read() ) && add_buffer.read() == 0){
		read_av.write(0);
	} else {
		read_av.write(1);
	}
}

void DMNI::receive()
{
	if(reset){
		tick_cnt = 0;
		first.write(0);
		last.write(0);
		SR.write(HEADER);
		add_buffer.write(0);
		receive_active.write(0);
		DMNI_Receive.write(WAIT);
		intr_count.write(0);
		for(int i=0; i<BUFFER_SIZE; i++){ //in vhdl replace by OTHERS=>'0'
			is_header[i] = 0;
			is_eop[i] = 0;
		}
		return;
	}

	tick_cnt++;

	sc_uint<4> intr_counter_temp = intr_count.read();

	//Read from NoC
	if (rx.read() == 1 && slot_available.read() == 1){

		if (SR.read() != INVALID){
			buffer[last.read()].write(data_in.read());
			is_eop[last.read()].write(eop_in.read());
			is_header[last.read()].write(SR.read() == HEADER);
			add_buffer.write(1);
			last.write(last.read() + 1);
		}

		switch (SR.read()) {
			case HEADER:
				noc_time = tick_cnt;
				SR.write(SIZE);
			break;
			case SIZE:
				flit_cntr = 2;
				if (data_in.read() < 11) {
					is_eop[last.read() - 1] = 0;
					is_header[last.read() - 1] = 0;
					add_buffer.write(0);
					last.write(last.read() - 1);
					if (eop_in.read()){
						is_eop[last.read()] = 0;
						SR.write(HEADER);
					} else {
						cout << "[DMNI] Dropping invalid packet size" << endl;
						SR.write(INVALID);
					}
				} else {
					intr_counter_temp = intr_counter_temp + 1;
					SR.write(PAYLOAD);
				}
				break;
			case PAYLOAD:
				flit_cntr++;		
				if(flit_cntr == 3) {
					is_delivery = (data_in.read() == 1);
				} else if(flit_cntr == 4){
					producer = data_in.read();
				} else if(flit_cntr == 5){
					consumer = data_in.read();
				} else if(flit_cntr == 7){
					timestamp = data_in.read();
				}
				if (eop_in.read() == 1){
					SR.write(HEADER);
					if(is_delivery){
						fstream dmni_log(path+"/debug/dmni.log", fstream::out | fstream::app);
						dmni_log << 
							tick_cnt << 
							'\t' << 
							(tick_cnt - timestamp) << 
							'\t' << 
							consumer << 
							'\t' << 
							producer << 
							'\t' <<
							(noc_time - timestamp) <<
							endl;
						dmni_log.flush();
					}
				}

			break;
			case INVALID:
				if (eop_in.read() == 1)
					SR.write(HEADER);
				break;
		}
	}

	//Write to memory
	switch (DMNI_Receive.read()) {

		case WAIT:

			if (start.read() == 1 && operation.read() == 1){
				read_flits.write(0);
				recv_address.write(address.read() - WORD_SIZE);
				recv_size.write(size.read() - 1);
				if (is_header[first.read()] == 1 && intr_counter_temp > 0){
					intr_counter_temp = intr_counter_temp - 1;
				}
				receive_active.write(1);
				
				if(address.read() == 0)
					DMNI_Receive.write(DROP_MESSAGE);
				else
					DMNI_Receive.write(COPY_TO_MEM);
			}
		break;

		case COPY_TO_MEM:

			if (write_enable.read() == 1 && read_av.read() == 1){
				noc_byte_we.write(0xF);

				noc_data_write.write(buffer[first.read()].read());
				first.write(first.read() + 1);
				add_buffer.write(0);
				recv_address.write(recv_address.read() + WORD_SIZE);
				recv_size.write(recv_size.read() - 1);
				read_flits.write(read_flits.read() + 1);

				if (recv_size.read() == 0 || is_eop[first.read()].read()){
					DMNI_Receive.write(END);
				}
			} else {
				noc_byte_we.write(0);
			}

		break;

		case DROP_MESSAGE:

			noc_byte_we.write(0);
			noc_data_write.write(0);
			recv_address.write(0);

			if (read_av.read() == 1){
				first.write(first.read() + 1);
				add_buffer.write(0);
				read_flits.write(read_flits.read() + 1);
				
				recv_size.write(recv_size.read() - 1);
				if (is_eop[first.read()].read()){
					cout << "[DMNI] Dropped " << read_flits.read() +1 << " flits" << endl;
					DMNI_Receive.write(END);
				}
			}

		break;

		case END:
			receive_active.write(0);
			noc_byte_we.write(0);
			recv_address.write(0);
			recv_size.write(0);
			DMNI_Receive.write(WAIT);
		break;

		default:
			break;
	}

	//Interruption management
	if (intr_counter_temp > 0){
		intr.write(1);
	} else {
		intr.write(0);
	}
	intr_count.write(intr_counter_temp);
}

void DMNI::send()
{
	if(reset){
		DMNI_Send.write(WAIT);
		send_active.write(0);
		tx.write(0);
		eop_out.write(0);
		return;
	}

	switch (DMNI_Send.read()) {
		case WAIT:
			if (start.read() == 1 && operation.read() == 0){
				send_address.write(address.read());
				send_address_2.write(address_2.read());
				send_size.write(size.read());
				send_size_2.write(size_2.read());
				send_active.write(1);
				DMNI_Send.write(LOAD);
								
				with_error = (
					(tick_counter.read() > ERR_INJECTION_BEGIN) 
					&& (tick_counter.read() < ERR_INJECTION_END)
					&& ((std::rand() % 100) < ERR_INJECTION_PROB_GLOBAL)
				);
			}
		break;

		case LOAD:

			if (credit_i.read() == 1 && read_enable.read() == 1){
				send_address.write(send_address.read() + WORD_SIZE);
				DMNI_Send.write(COPY_FROM_MEM);
			}
		break;

		case COPY_FROM_MEM:

			if (credit_i.read() == 1 && read_enable.read() == 1){

				if (send_size.read() > 0){

					tx.write(1);
					send_address.write(send_address.read() + WORD_SIZE);
					send_size.write(send_size.read() - 1);

					unsigned err_mask = 0;
					if (
						with_error 
						&& send_size.read() != 13 // target
						&& send_size.read() != 12 // size
						&& send_size.read() != 11 // service
						&& send_size.read() != 10 // prod_task
						&& send_size.read() != 9 // cons_task
						&& send_size.read() != 5 // msg_length
						&& send_size.read() != 4 // prod_addr
						&& (std::rand() % 100) < ERR_INJECTION_PROB_FLIT
					) {
						for (int i = 0; i < 32; i++) {
							if ((std::rand() % 100) < ERR_INJECTION_PROB_BIT) {
								err_mask |= (1 << i);
							}
						}
						if (err_mask)
							std::cout << std::hex << "Injecting error in bits " << err_mask << std::endl;
					}

					data_out.write(mem_data_read.read() ^ err_mask);

					// if(address_router == 1)
					// 	cout << "[DMNI HW] " << mem_data_read.read() << endl;
					if (send_size.read() == 1 && send_size_2.read() == 0)
						eop_out.write(1);

				} else if (send_size_2.read() > 0) {

					send_size.write(send_size_2.read());
					send_size_2.write(0);
					tx.write(0);
					if (send_address_2.read()(30,28) == 0){
						send_address.write(send_address_2.read());
					} else {
						send_address.write(send_address_2.read() - WORD_SIZE);
					}
					DMNI_Send.write(LOAD);

					if (send_size_2.read() == 1)
						eop_out.write(1);

				} else {
					tx.write(0);
					eop_out.write(0);
					DMNI_Send.write(END);
				}
			} else {

				if (credit_i.read() == 0){
					send_size.write(send_size.read() + 1);
					send_address.write(send_address.read() - (WORD_SIZE + WORD_SIZE)); // endereco volta 2 posicoes
				} else {
					send_address.write(send_address.read() - WORD_SIZE);  // endereco volta 1 posicoes
				}
				tx.write(0);
				DMNI_Send.write(LOAD);
			}

		break;

		case END:
			eop_out.write(0);
			send_active.write(0);
			send_address.write(0);
			send_address_2.write(0);
			send_size.write(0);
			send_size_2.write(0);
			DMNI_Send.write(WAIT);
		break;

		default:
			break;
	}
}

void DMNI::br_receive()
{
	if(reset){
		br_byte_we = 0;
		br_ack_mon = false;
		br_rcv_state = BR_RCV_IDLE;
		for(int n = 0; n < N_PE; n++){
			for(int t = 0; t < TASK_PER_PE; t++){
				mon_table[n][t] = -1;
			}
		}

		return;
	}

	if(!br_req_mon)
		br_ack_mon = false;
	else if(monitor_ptrs[br_mon_svc] == 0)	/* Ignore invalid requests */
		br_ack_mon = true;

	switch(br_rcv_state){
		case BR_RCV_IDLE:
		{
			if(br_rcv_enable){
				br_mem_data = br_payload;

				uint32_t ptr = monitor_ptrs[br_mon_svc];
				uint16_t src = br_address >> 16;
				uint16_t seq_addr = (src >> 8) + (src & 0xFF)*N_PE_X;
				ptr += (seq_addr * TASK_PER_PE * 8);

				uint16_t task = br_producer;

				int idx = -1;
				for(int i = 0; i < TASK_PER_PE; i++){
					if(mon_table[seq_addr][i] == task){
						idx = i;
						break;
					}
				}

				if(idx != -1){
					ptr += (idx * 8 + 4);
					br_mem_addr = ptr;

					br_byte_we = 0xF;
					br_rcv_state = BR_RCV_TASKID;
				} else {
					for(int i = 0; i < TASK_PER_PE; i++){
						if(mon_table[seq_addr][i] == -1){
							idx = i;
							break;
						}
					}

					if(idx != -1){
						mon_table[seq_addr][idx] = task;
						ptr += (idx * 8 + 4);
						br_mem_addr = ptr;

						br_byte_we = 0xF;
						br_rcv_state = BR_RCV_TASKID;
					} else {
						cout << "ERROR: NO AVAILABLE SPACE IN MONITOR LUT -- NEED CLEANUP" << endl;
						br_rcv_state = BR_RCV_END;
						br_ack_mon = true;
					}
				}
			}
			break;
		}
		case BR_RCV_TASKID:
		{
			br_mem_data = br_producer;

			uint32_t ptr = br_mem_addr - 4;
			br_byte_we = 0xF;

			br_mem_addr = ptr;
			br_ack_mon = true;
			br_rcv_state = BR_RCV_END;
			break;
		}
		case BR_RCV_END:
		{
			br_byte_we = 0;
			br_rcv_state = BR_RCV_IDLE;
			break;
		}
	}

	/**
	 * @todo
	 * Optimize this with 64 bit payload (add source PE to addressing)
	 */
	if(clear_task){
		int16_t task_to_clear = config_data.read();
		bool found = false;
		for(int n = 0; n < N_PE; n++){
			for(int t = 0; t < TASK_PER_PE; t++){
				if(mon_table[n][t] == task_to_clear){
					mon_table[n][t] = -1;
					found = true;
					// cout << "Cleared task " << task_to_clear << " from table PE " << n << " id " << t << endl;
					break;
				}
			}
			if(found)
				break;
		}
	}
}
