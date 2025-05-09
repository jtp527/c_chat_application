import socket
import threading
import tkinter as tk
from tkinter import simpledialog, scrolledtext
from datetime import datetime

# Constants for server connection
HOST = "127.0.0.1"
PORT = 12345

class ChatClient:
    def __init__(self, master):
        self.master = master
        self.master.title("Python GUI Chat Client")
        self.running = False

        # Build UI components
        self._setup_ui()
        if not getattr(self, 'username', None):
            self.master.destroy()
            return

        # Attempt initial connect to server
        self._attempt_connect()

    def _setup_ui(self):
        # Prompt for username
        self.username = simpledialog.askstring(
            "Username", "Enter your username:", parent=self.master)
        # If user cancelled
        if not self.username:
            return
        # Enforce maximum length via same prompt style
        while len(self.username) >= 32:
            self.username = simpledialog.askstring(
                "Username Error",
                "Username must be less than 32 characters.\nEnter another username:",
                parent=self.master)
            if not self.username:
                return

        # Chat display area
        self.chat_area = scrolledtext.ScrolledText(
            self.master, wrap=tk.WORD)
        self.chat_area.pack(padx=10, pady=10, fill=tk.BOTH, expand=True)
        self.chat_area.config(state='disabled')

        # Input box for messages
        self.input_box = tk.Entry(self.master)
        self.input_box.pack(padx=10, pady=(0, 5), fill=tk.X)
        self.input_box.bind("<Return>", self.send_message)
        self.input_box.config(state='disabled')

        # Button frame
        btn_frame = tk.Frame(self.master)
        btn_frame.pack(padx=10, pady=(0, 5), anchor='e')

        # Disconnect button
        self.disconnect_button = tk.Button(
            btn_frame, text="Disconnect", command=self.disconnect)
        self.disconnect_button.pack(side=tk.RIGHT, padx=(5,0))
        self.disconnect_button.config(state='disabled')

        # Reconnect button
        self.reconnect_button = tk.Button(
            btn_frame, text="Reconnect", command=self.reconnect)
        self.reconnect_button.pack(side=tk.RIGHT)
        self.reconnect_button.config(state='disabled')

        # Status bar at bottom
        self.status = tk.Label(
            self.master, text="Disconnected",
            bd=1, relief=tk.SUNKEN, anchor=tk.W)
        self.status.pack(side=tk.BOTTOM, fill=tk.X)

    def _attempt_connect(self):
        # Handshake loop with server validation
        while True:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            try:
                sock.connect((HOST, PORT))
            except Exception:
                self.show_message("Unable to reach server.")
                sock.close()
                return

            # send username and wait server response
            sock.send(self.username.encode())
            try:
                resp = sock.recv(1024).decode().strip()
            except Exception:
                self.show_message("Handshake failed.")
                sock.close()
                return

            # check response
            if resp.startswith("Welcome"):
                self.client_socket = sock
                self.show_message(resp)
                break
            else:
                # show error and prompt new username
                self.show_message(resp)
                sock.close()
                new_name = simpledialog.askstring(
                    "Username Error", resp + "\nEnter another username:",
                    parent=self.master)
                if not new_name:
                    return
                # enforce length again
                while len(new_name) >= 32:
                    new_name = simpledialog.askstring(
                        "Username Error",
                        "Username must be less than 32 characters.\nEnter another username:",
                        parent=self.master)
                    if not new_name:
                        return
                self.username = new_name

        # On successful handshake
        self.status.config(text="Connected")
        self.running = True
        self.input_box.config(state='normal')
        self.disconnect_button.config(state='normal')
        self.reconnect_button.config(state='disabled')
        self._start_receiver()

    def _start_receiver(self):
        thread = threading.Thread(
            target=self.receive_messages,
            daemon=True
        )
        thread.start()

    def show_message(self, message):
        self.chat_area.config(state='normal')
        self.chat_area.insert(tk.END, message + "\n")
        self.chat_area.config(state='disabled')
        self.chat_area.see(tk.END)

    def send_message(self, event=None):
        if not self.running:
            return
        message = self.input_box.get().strip()
        if message:
            timestamp = datetime.now().strftime('%H:%M')
            formatted = f"[{timestamp}] You: {message}"
            self.show_message(formatted)
            try:
                self.client_socket.send(message.encode())
                self.input_box.delete(0, tk.END)
            except Exception:
                self.show_message("Failed to send message.")
                self.disconnect()

    def receive_messages(self):
        buffer = ""
        while self.running:
            try:
                data = self.client_socket.recv(1024)
                if not data:
                    self.show_message("Server disconnected.")
                    break

                buffer += data.decode()

                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.rstrip()
                    if line:
                        self.show_message(line)
                        self.master.bell()
            except Exception:
                break

        # Clean up on disconnect
        self.disconnect(silent=True)

    def disconnect(self, silent=False):
        # notify server and close
        if self.running:
            try:
                self.client_socket.send(b"/quit")
            except:
                pass
        self.running = False
        try:
            self.client_socket.shutdown(socket.SHUT_RDWR)
            self.client_socket.close()
        except:
            pass

        self.status.config(text="Disconnected")
        self.input_box.config(state='disabled')
        self.disconnect_button.config(state='disabled')
        self.reconnect_button.config(state='normal')
        if not silent:
            self.show_message("Disconnected from server.")

    def reconnect(self):
        self.show_message("Reconnecting...")
        self._attempt_connect()

    def on_close(self):
        self.disconnect(silent=True)
        self.master.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    client = ChatClient(root)
    root.protocol("WM_DELETE_WINDOW", client.on_close)
    root.mainloop()
