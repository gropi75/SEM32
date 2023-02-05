

// initialize RS485 bus for Soyosource communication
void Soyosource_init_RS485(uint8_t rx_pin, uint8_t tx_pin, uint8_t en_pin);

// Function prepares serial string and sends powerdemand by serial interface  to Soyo
// needs as input also the enable pin of the RS485 interface
void sendpower2soyo (short demandpowersend, uint8_t en_pin);

