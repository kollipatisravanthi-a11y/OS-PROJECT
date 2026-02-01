# Use Python 3.11 as base
FROM python:3.11-slim

# Install system dependencies (gcc, libc, etc.)
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy requirements and install
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy project files
COPY . .

# Ensure empty __init__.py exists for package recognition
RUN touch Backend/__init__.py

# Expose port (Railway provides $PORT)
ENV PORT=8080
EXPOSE 8080

# Start command
CMD gunicorn --bind 0.0.0.0:$PORT Backend.app:app
