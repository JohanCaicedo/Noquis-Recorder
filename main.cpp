#include "Application.hpp"
#include <iostream>

int main() {
    std::cout << "Iniciando AI-Link Capture..." << std::endl;

    try {
        // Pasamos -1 para que el menu grafico sea el punto de entrada.
        // El usuario seleccionara su capturadora desde dentro del programa.
        Application app(-1);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error fatal en la aplicacion: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}