import socket
import threading

from typing import Any
from simulator import GridSimulator


class Server():
    def __init__(self, port: int) -> None:
        self.simulator = GridSimulator()
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.threads = {}

        self.port = port
        self.server_socket.setsockopt(
            socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        self.server_socket.bind(('', port))  # REMEMBER TO SET OWN I

        # Listens for 1 connection. Should be set up equal to the amount of households in the grid
        self.server_socket.listen(1)

    def __call__(self, *args: Any, **kwds: Any) -> Any:
        print("Server started listening on %s:%d" % ('0.0.0.0', self.port))
        while True:
            try:
                socket_obj, addr = self.server_socket.accept()

                if addr[0] in self.threads.keys():
                    # Free PNI used from thread
                    for used_nodes in self.simulator.taken_nodes:
                        if used_nodes[0] == addr[0]:
                            self.simulator.available_nodes.append(
                                used_nodes[1])
                            self.simulator.available_nodes.sort()
                    self.simulator.taken_nodes = [
                        nodes for nodes in self.simulator.taken_nodes if addr[0] not in nodes[0]]
                    # thread = self.threads.get(addr[0])

                print(
                    f"Household connected at {addr}. Redirecting household to thread..")
                thread = threading.Thread(
                    target=self.thread_handler, args=(socket_obj, addr))
                thread.start()
                self.threads.update({addr[0]: thread})

            except KeyboardInterrupt:
                self.simulator.oc.exit()
                break

            except Exception as e:
                print(e)
                continue

    def thread_handler(self, *args):
        LASET = args[0]
        LASET_addr = args[1]

        while True:
            try:
                print("Awaiting content from household %s " % (LASET_addr,))
                content = LASET.recv(1024).decode('utf-8')

                # Get header from content
                header = content.split(',')[0]

                match header:
                    case 'rni':
                        node_id = self.simulator.get_node_id(LASET_addr)
                        message = f'pni,{node_id};'

                    case 'rql':
                        node_id = content.split(',')[1]
                        amperage = self.simulator.get_amperage_by_node(node_id)
                        message = f'par,{amperage};'

                    case 'rlc':
                        amp1, amp2, amp3, buyer_index, seller_index, offer = content.split(',')[
                            1:]
                        amps_from_esp = [
                            0, float(amp1), 0, float(amp2), float(amp3)]

                        estimated_grid = self.simulator.start_validation(
                            int(buyer_index), int(seller_index), float(offer), amps_from_esp)

                        message = f'plc,{estimated_grid};'
                    case _:
                        print("Invalid header %s received." % (header))
                        # Punishment: Close thread and socket connection
                        LASET.close()
                        break

                # Send node ID back to household
                LASET.sendall(message.encode('utf-8'))
                print("Sent %s to household (node %s)" %
                      (message, node_id))
                print("")

            except KeyboardInterrupt:
                self.server_socket.close()
                self.threads.join()
                print("Exiting, closing socket and threads")
                break

            except Exception as e:
                print(
                    f"Unexpected error occured. Closing thread... reason: {e}")
                break


if __name__ == '__main__':
    server = Server(6666)

    # Simulate one load
    server.simulator.update_amperage([0, -5, 0, 25, 15])
    server()
