const os = require('os');

function getLocalIP() {
    const interfaces = os.networkInterfaces();
    
    for (const name of Object.keys(interfaces)) {
        for (const iface of interfaces[name]) {
            // Skip internal (localhost) and non-IPv4 addresses
            if (iface.family === 'IPv4' && !iface.internal) {
                return iface.address;
            }
        }
    }
    
    return 'localhost';
}

const localIP = getLocalIP();
console.log('IP do seu computador:', localIP);
console.log('Atualize o ESP32 com este IP:');
console.log(`const char* serverURL = "http://${localIP}:3000";`);
