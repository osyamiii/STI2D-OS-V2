import os, tempfile, unittest
from fastapi.testclient import TestClient

class RelayTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        os.environ['STI2D_MESSAGES_DB'] = self.tmp.name + '/test.sqlite3'
        os.environ['STI2D_INVITE_CODE'] = 'test-invite-code'
        from messaging.server import app as relay
        relay.DB_PATH = __import__('pathlib').Path(os.environ['STI2D_MESSAGES_DB'])
        relay.INVITE_CODE = os.environ['STI2D_INVITE_CODE']
        relay.init_db()
        self.client = TestClient(relay.app)
        self.a = self.client.post('/v1/register', json={'username':'alice','password':'a-very-long-password-1','invite_code':'test-invite-code'})
        self.b = self.client.post('/v1/register', json={'username':'bob','password':'another-long-password-2','invite_code':'test-invite-code'})
        self.assertEqual(self.a.status_code, 200)
        self.assertEqual(self.b.status_code, 200)
        self.ta = self.a.json()['access_token']; self.tb = self.b.json()['access_token']
    def tearDown(self): self.tmp.cleanup()
    def test_auth_and_delivery(self):
        sent = self.client.post('/v1/messages', headers={'Authorization':'Bearer '+self.ta}, json={'to':'bob','text':'Bonjour Bob','client_id':'unique-client-id-0001'})
        self.assertEqual(sent.status_code, 200, sent.text)
        inbox = self.client.get('/v1/inbox', headers={'Authorization':'Bearer '+self.tb})
        self.assertEqual(inbox.status_code, 200)
        self.assertEqual(inbox.json()['messages'][0]['text'], 'Bonjour Bob')
        self.assertEqual(inbox.json()['messages'][0]['from'], 'alice')
    def test_duplicate_client_id_is_idempotent(self):
        payload={'to':'bob','text':'Une seule fois','client_id':'same-client-id-0001'}
        h={'Authorization':'Bearer '+self.ta}
        one=self.client.post('/v1/messages',headers=h,json=payload).json()['message']['id']
        two=self.client.post('/v1/messages',headers=h,json=payload).json()['message']['id']
        self.assertEqual(one,two)
        self.assertEqual(len(self.client.get('/v1/inbox',headers={'Authorization':'Bearer '+self.tb}).json()['messages']),1)
    def test_client_id_cannot_be_reused_for_different_payload(self):
        h={'Authorization':'Bearer '+self.ta}
        first={'to':'bob','text':'Original','client_id':'same-client-id-0002'}
        self.assertEqual(self.client.post('/v1/messages',headers=h,json=first).status_code,200)
        changed={'to':'bob','text':'Modified','client_id':'same-client-id-0002'}
        self.assertEqual(self.client.post('/v1/messages',headers=h,json=changed).status_code,409)
    def test_reject_bad_invite_and_invalid_recipient(self):
        bad=self.client.post('/v1/register',json={'username':'charlie','password':'a-very-long-password-3','invite_code':'wrong'})
        self.assertEqual(bad.status_code,403)
        missing=self.client.post('/v1/messages',headers={'Authorization':'Bearer '+self.ta},json={'to':'nobody','text':'hi','client_id':'unique-client-id-0002'})
        self.assertEqual(missing.status_code,404)
    def test_live_delivery(self):
        with self.client.websocket_connect('/v1/live', headers={'Authorization':'Bearer '+self.tb}) as ws:
            ready = ws.receive_json()
            self.assertEqual(ready['type'], 'ready')
            sent = self.client.post('/v1/messages', headers={'Authorization':'Bearer '+self.ta}, json={'to':'bob','text':'En direct','client_id':'unique-client-id-live'})
            self.assertEqual(sent.status_code, 200)
            pushed = ws.receive_json()
            self.assertEqual(pushed['type'], 'message')
            self.assertEqual(pushed['message']['text'], 'En direct')
    def test_delete_message(self):
        sent=self.client.post('/v1/messages',headers={'Authorization':'Bearer '+self.ta},json={'to':'bob','text':'A supprimer','client_id':'unique-client-id-0003'}).json()['message']
        deleted=self.client.delete('/v1/messages/'+str(sent['id']),headers={'Authorization':'Bearer '+self.tb})
        self.assertEqual(deleted.status_code,200)
        self.assertEqual(self.client.get('/v1/inbox',headers={'Authorization':'Bearer '+self.tb}).json()['messages'],[])

if __name__ == '__main__': unittest.main()
