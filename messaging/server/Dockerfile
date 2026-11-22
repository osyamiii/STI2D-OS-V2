FROM python:3.12-slim
WORKDIR /app
COPY messaging/requirements.txt /app/requirements.txt
RUN pip install --no-cache-dir -r requirements.txt
COPY messaging/server/app.py /app/app.py
ENV STI2D_MESSAGES_DB=/data/messages.sqlite3
EXPOSE 8765
CMD ["uvicorn", "app:app", "--host", "0.0.0.0", "--port", "8765", "--proxy-headers"]
