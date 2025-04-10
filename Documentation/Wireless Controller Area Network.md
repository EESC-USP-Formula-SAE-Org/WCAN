O melhor approach para criar isso na real é um sistema sem mestre, onde todo mundo pode mandar e receber dados. Idealmente como a rede can funciona

Essencialmente a gente faz um broadcast do dado e todo mundo fica ouvindo. Lá a gente faz um filtro onde nos dados existe um `can_id` que é usado para aceitar ou recusar aquele dado.
Para fazer isso vamos precisar fazer um setup bem simples, onde temos um receive callback que recebe tudo, e ele vai até o processo de interpretar o dado e olhar se o `can_id` é de interesse. Se for adicionamos o pacote de dados numa fila para ser processado por uma outra task. É interessante fazer algo tipo os leds de RX e TX que piscam quando dado chega e é enviado.

No nosso sistema existem dois tipos de nós, um que comunica com a rede CAN e outro que aquisita dados, ambos enviam informações:

- Central:
	- envia:
		- tempo atual
	- recebe:
		- dados de sensores, pega todos que recebeu e envia direto para rede can, sem filtro
- Aquisitor:
	- envia:
		- dado coletado (só deve começar a enviar depois de receber o tempo atual)
	- recebe:
		- dado do tempo atual

Para o sistema geral vamos precisar construir o seguinte:
Um sistema de setup para ligar o esp now para enviar e receber.
Um receive callback que faz a filtragem via uma lista de can_id's (se a lista for vazia não filtra) e monta uma fila de dados
Uma tarefa de baixa prioridade que processa os dados da fila (esse cara é diferente para todo mundo), o melhor aqui vai ser fazer uma task geral que fica olhando pra lista e ela chama alguma outra função para fazer esse processamento. idealmente no setup a gente cadastra qual é essa função
Uma função para enviar um dado com um certo can_id

Para facilitar minha vida primeiro vou fazer um sistema geral sem essa noção de central e aquisitor. vou fazer uma centralzona que fica trocando entre si o tempo atual que ela ta marcando