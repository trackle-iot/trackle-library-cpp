
""" Non-blocking SSE client. """

import threading
import queue

import sseclient

class SSEClientNoWait:

    """ SSE client that exposes a non-blocking pop to get next received message """

    @staticmethod
    def with_requests(url, headers):
        """Get a streaming response for the given event feed using requests."""
        import requests
        return requests.get(url, stream=True, headers=headers)

    def __init__(self, *args, **kargs):
        self.url = args[0]  # Supponiamo che il primo argomento sia l'url
        self.headers = kargs.get('headers', {})  # Se non ci sono headers, assegna un dizionario vuoto
        response = self.with_requests(self.url, self.headers)

        self.__sseclient = sseclient.SSEClient(response)
        self.__thread = threading.Thread(target=self.__thread_code, daemon=True)
        self.__queue = queue.Queue()
        self.__thread.start()

    def __thread_code(self):
        for event in self.__sseclient.events():
            self.__queue.put(event)

    def clear_pending_events(self):
        """ Discard events received but not popped yet """
        try:
            while not self.__queue.empty():
                self.__queue.get_nowait()
        except queue.Empty:
            pass

    def pop_nowait(self) -> sseclient.Event:
        """ Pop next received event. If none, raise queue.Empty """
        return self.__queue.get_nowait()
    