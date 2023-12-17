from validator import validate, initialize_octave


class GridSimulator():
    """
    Simulate voltage and current in each household of a neighbourhood
    """

    def __init__(self) -> None:
        self.V_0 = 230      # Base voltage at the transformator

        # USING RASMUS's expanded grid (with household 7 also)
        self.system = [     # [source node ID, destination node ID, resistance]
            [1, 2, .01],
            [2, 3, .5],
            [2, 4, .3],
            [4, 5, .2],
            [4, 6, .1],]
        # The actual amperage in Ampere's per household and each intersection. Cable ID := node ID - 1
        # [nodeID,I_amperage]
        self.I_amperage = [[2, 0], [3, 0], [4, 0], [5, 0], [6, 0]]
        self.available_nodes = [3, 5, 6]
        self.taken_nodes = []

        # init octave!
        self.oc = initialize_octave()

    def update_amperage(self, amperage: list[int]):
        """
        Update amperage in amps per household and calculate new grid values (Cable ID := node ID - 1)
        """
        for index, _ in enumerate(self.I_amperage):
            self.I_amperage[index][1] = amperage[index]

    def get_amperage_by_node(self, node_id: int):
        """
        Gets the amperage in amps for a specific household based on its node id - Used for the server reply to household!
        """
        for amperage in self.I_amperage:
            if amperage[0] == int(node_id):
                return amperage[1]

    def get_amperage(self):
        """
        Gets the amperage in amps without household node id attached.
        """
        I_amperage = []
        for amperage in self.I_amperage:
            I_amperage.append(amperage[1])

        return I_amperage

    def get_node_id(self, addr):
        """
        Returns next available node ID and assigns this to the node that requested it.
        """
        if not self.available_nodes:
            return -1

        if addr in self.taken_nodes:
            return -1

        self.taken_nodes.append([addr[0], self.available_nodes[0]])

        # Assign node id
        return self.available_nodes.pop(0)

    # bi = buyer_index, si = seller_index, of = offer
    def start_validation(self, bi: int, si: int, of: int, amperage: list):
        """
        """
        # I_amperage_in_kWh = self.amperage_converter(amperage, self.V_0)
        parameters = {
            'base_voltage': self.V_0,  # total voltage
            'I_amperage': amperage,  # load
            'buyer_index': bi,  # node_id for buyer
            'seller_index': si,  # node_id for seller
            'offer': of,  # kr. pr. kwh but its amperage as of right now - provided by the agreed amount in the trade deal
            'system': self.system
        }

        result = validate(self.oc, **parameters)
        return result

    def amperage_converter(self, I_amperage: list, base_voltage: int, time: float):
        """
        Time is given by ESP32 in seconds with the different amperages. These are converted into kWh.
        """
        I_amperage_in_kWh = []
        hours = (time / 60) / 60
        for amperage in I_amperage:
            W = amperage * base_voltage
            kW = W / 1000
            kWh = kW * hours
            I_amperage_in_kWh.append(kWh)

        return I_amperage_in_kWh

if __name__ == '__main__':
    grid = GridSimulator()
