#!/bin/bash

echo "🐳 Building Docker image..."
docker build -t ft_irc_test .

echo ""
echo "🔨 Compiling in Docker..."
docker run --rm -v $(pwd):/app ft_irc_test make re

if [ $? -eq 0 ]; then
    echo "✅ Compilation successful on Linux!"
else
    echo "❌ Compilation failed on Linux!"
    exit 1
fi

echo ""
echo "🧪 Running basic server test..."
docker run -d --name irc_test -v $(pwd):/app -p 6667:6667 ft_irc_test ./ircserv 6667 testpass

sleep 2

echo "Testing connection..."
printf "HELLO\r\n" | nc localhost 6667

docker stop irc_test 2>/dev/null
docker rm irc_test 2>/dev/null

echo "✅ Docker test complete!"
