import importlib.util, pathlib, unittest
p=pathlib.Path(__file__).parents[1]/'companion'/'sti2d_messages.py'
spec=importlib.util.spec_from_file_location('sti2d_messages',p); mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
class ProtocolTests(unittest.TestCase):
    def test_round_trip_frame(self):
        frame=mod.serial_packet({'v':1,'type':'send','id':'abc123456','to':'alice','text':'Salut'})
        self.assertTrue(frame.endswith(b'\n'))
        self.assertEqual(mod.parse_device_line(frame.strip()),('alice','Salut','abc123456'))
    def test_reject_unknown_version_and_oversize(self):
        with self.assertRaises(ValueError): mod.parse_device_line(b'{"v":2,"type":"send","to":"alice","text":"x"}')
        with self.assertRaises(ValueError): mod.serial_packet({'x':'a'*5000})
    def test_reject_invalid_message(self):
        with self.assertRaises(ValueError): mod.parse_device_line(b'{"v":1,"type":"send","to":"alice","text":"  "}')
if __name__ == '__main__': unittest.main()
