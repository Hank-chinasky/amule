using System;
using System.IO;
using System.Security;
using System.Security.Permissions;
using System.Threading;
using System.Collections.Generic;
using System.Text;
using System.Net;
using System.Net.Sockets;

namespace amule.net
{
    class amuleSocket {
        Socket sock;

        bool ReadNumber(ref UInt16 i)
        {
            return false;
        }

        public amuleSocket(Socket s)
        {
            sock = s;
        }
    }

    // Define the state object for the callback. 
    // Use hostName to correlate calls with the proper result.
    public class ResolveState {
        IPHostEntry resolvedIPs;

        public string errorMsg;

        public IPHostEntry IPs
        {
            get { return resolvedIPs; }
            set { resolvedIPs = value; }
        }
    }

    public class ConnectState {
        public Socket sock;
        public string errorMsg;

        public ConnectState(Socket s)
        {
            sock = s;
        }
    }

    class amuleRemote {
        static ManualResetEvent m_op_Done = new ManualResetEvent(false);

        // Record the IPs in the state object for later use.
        static void GetHostEntryCallback(IAsyncResult ar)
        {
            ResolveState ioContext = (ResolveState)ar.AsyncState;
            try {
                ioContext.IPs = Dns.EndGetHostEntry(ar);
            } catch (SocketException e) {
                ioContext.errorMsg = e.Message;
            }
            m_op_Done.Set();
        }

        UInt32 flags;

        Socket m_s;

        static void ConnectCallback(IAsyncResult ar)
        {
            // Retrieve the socket from the state object.
            ConnectState state = (ConnectState)ar.AsyncState;
            try {
                // Complete the connection.
                state.sock.EndConnect(ar);

                Console.WriteLine("Socket connected to {0}",
                    state.sock.RemoteEndPoint.ToString());

                // Signal that the connection has been made.
                m_op_Done.Set();
            } catch (Exception e) {
                state.errorMsg = e.Message;
            }
        }

        //byte [] m_rx_buffer;
        byte[] m_tx_buffer = null;
        MemoryStream m_tx_mem_stream;

        //LinkedList<byte[]> m_tx_queue;
        BinaryReader m_sock_reader = null;
        BinaryWriter m_sock_writer = null;
        NetworkStream m_socket_stream = null;

        static void RxCallback(IAsyncResult ar)
        {
        }

        static void TxCallback(IAsyncResult ar)
        {
            amuleRemote o = (amuleRemote)ar;
            Console.WriteLine("TxCallback signalled, calling EndWrite");
            o.m_s.EndSend(ar);
            m_op_Done.Set();
        }

        public bool SendPacket(ecProto.ecPacket packet)
        {
            return true;
        }

        [DnsPermission(SecurityAction.Demand, Unrestricted = true)]
        public bool ConnectToCore(string host, int port, string login, ref string error)
        {
            try {
                m_op_Done.Reset();
                ResolveState resolveContext = new ResolveState();
                Dns.BeginGetHostEntry(host,
                    new AsyncCallback(GetHostEntryCallback), resolveContext);

                // Wait here until the resolve completes (the callback calls .Set())
                m_op_Done.WaitOne();
                if ( resolveContext.IPs == null ) {
                    error = resolveContext.errorMsg;
                    return false;
                }
                Console.WriteLine("Resolved: '{0}' -> '{1}", host,resolveContext.IPs.AddressList[0]);
                Console.WriteLine("Connecting to {0}:{1}", resolveContext.IPs.AddressList[0], port);
                IPEndPoint remoteEP = new IPEndPoint(resolveContext.IPs.AddressList[0], port);
                m_s = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);

                m_op_Done.Reset();
                ConnectState connectContext = new ConnectState(m_s);
                m_s.BeginConnect(remoteEP, new AsyncCallback(ConnectCallback), connectContext);
                m_op_Done.WaitOne(1000,true);

                if ( connectContext.errorMsg != null) {
                    error = connectContext.errorMsg;
                    return false;
                }
                m_socket_stream = new NetworkStream(m_s);

                m_sock_reader = new BinaryReader(m_socket_stream);

                m_tx_buffer = new byte[32*1024];

                m_tx_mem_stream = new MemoryStream(m_tx_buffer);
                m_sock_writer = new BinaryWriter(m_tx_mem_stream);
                ecProto.ecLoginPacket p = new ecProto.ecLoginPacket("amule.net", "0.0.1", "12345");
                p.Write(m_sock_writer);

                m_op_Done.Reset();
                m_s.BeginSend(m_tx_buffer, 0, p.Size(), 0, new AsyncCallback(TxCallback), this);
                m_op_Done.WaitOne(1000, true);

                //m_s.BeginReceive(m_rx_buffer, 0, new AsyncCallback(RxCallback), this);

            } catch (Exception e) {
                error = e.Message;
                return false;
            }
            return true;
        }

        public bool SendRequest()
        {
            return true;
        }
    }
}
