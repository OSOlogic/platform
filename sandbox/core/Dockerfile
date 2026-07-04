FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY oso_core.py .
EXPOSE 8080 4840
CMD ["python", "oso_core.py"]
