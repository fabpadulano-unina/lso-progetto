FROM gcc:13

# Directory di lavoro
WORKDIR /app

# Copia tutto il progetto
COPY . .

# Compila server e client
RUN make

# Porta del server
EXPOSE 8888

# Comando di default (può essere sovrascritto)
CMD ["bash"]
