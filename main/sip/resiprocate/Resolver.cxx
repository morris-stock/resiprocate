

#include <util/Socket.hxx>

#include <sys/types.h>

#ifndef WIN32
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif


#include <stdio.h>
#include <errno.h>

#include <sipstack/Symbols.hxx>
#include <sipstack/Resolver.hxx>
#include <sipstack/Uri.hxx>

#include <util/Data.hxx>
#include <util/Logger.hxx>
#include <util/ParseBuffer.hxx>

#define VOCAL_SUBSYSTEM Vocal2::Subsystem::SIP

using namespace Vocal2;


Resolver::Resolver(const Data& hostport) 
{
   ParseBuffer buf(hostport.data(), hostport.size());
   const char* start = buf.position();
   buf.skipToChar(Symbols::COLON[0]);
   mHost = buf.data(start);
   if (buf.eof())
   {
      mPort = -1;
   }
   else
   {
      buf.skipChar(Symbols::COLON[0]);
      mPort = buf.integer();
   }
}

Resolver::Resolver(const Data& host, int port, Transport::Type transport) : 
   mHost(host), 
   mPort(port),
   mTransport(transport)
{
}

Resolver::Resolver(const Uri& uri) : 
   mHost(uri.host()),
   mPort(uri.port() ? uri.port() : 5060)
{
   bool isNumeric = isIpAddress(mHost);
   if (!uri.exists(p_transport) )
   {
      if (isNumeric)
      {
         if (uri.scheme() == Symbols::Sip)
         {
            mTransport = Symbols::UDP;
         }
         else if (uri.scheme() == Symbols::Sips)
         {
            mTransport = Symbols::TCP;
         }
      }
      else // not numeric
      {
         if (1) // uri.portSpecified()) // !jf!
         {
            if (uri.scheme() == Symbols::Sip)
            {
               mTransport = Symbols::UDP;
            }
            else if (uri.scheme() == Symbols::Sips)
            {
               mTransport = Symbols::TCP;
            }
         }
         else // NAPTR query - yuck! 
         {
            // Rohan, step up to the plate buddy.
            mTransport = Symbols::UDP; // !jf! not done yet
         }
      }
   }

   if (!isNumeric)
   {
      if (1) // !jf! uri.portSpecified())
      {
         lookupARecords();
         // do an A or AAAA DNS lookup
         
      }
      else
      {
         // do SRV lookup on result of NAPTR lookup
         
         // if no NAPTR lookup, do SRV lookup on _sips for sips and _sip for sip
         
         // if no result on SRV lookup, do an A or AAAA lookup
         
      }
   }
}

void
Resolver::lookupARecords()
{
   struct hostent hostbuf; 
   struct hostent* result=0;

   int herrno=0;
   char buffer[8192];
#ifdef __QNX__
   result = gethostbyname_r (mHost.c_str(), &hostbuf, buffer, sizeof(buffer), &herrno);
   if (result == 0)
   {
#else

#ifdef WIN32
	assert(0); // !cj! 
	int ret = -1;
#else
   int ret = gethostbyname_r (mHost.c_str(), &hostbuf, buffer, sizeof(buffer), &result, &herrno);
#endif
   assert (ret != ERANGE);

   if (ret != 0)
   {
#endif
      switch (herrno)
      {
         case HOST_NOT_FOUND:
            InfoLog ( << "host not found: " << mHost);
            break;
         case NO_DATA:
            InfoLog ( << "no data found for: " << mHost);
            break;
         case NO_RECOVERY:
            InfoLog ( << "no recovery lookup up: " << mHost);
            break;
         case TRY_AGAIN:
            InfoLog ( << "try again: " << mHost);
            break;
      }
   }
   else
   {
      assert(result);
      assert(result->h_length == 4);
   
      DebugLog (<< "DNS lookup of " << mHost << ": canonical name: " << result->h_name);
      char str[256];
      for (char** pptr = result->h_addr_list; *pptr != 0; pptr++)
      {
         Resolver::Tuple tuple;
         memset(&tuple.ipv4, 0, sizeof(tuple.ipv4));
         tuple.ipv4.sin_family = AF_INET; //result->h_addrtype;
         tuple.ipv4.sin_port = htons(mPort);
         tuple.ipv4.sin_addr.s_addr = *((u_int32_t*)(*pptr));
         tuple.port = mPort;
         tuple.transport = mTransport;
         mNextHops.push_back(tuple);

         assert(mNextHops.back().ipv4.sin_addr.s_addr == tuple.ipv4.sin_addr.s_addr);
#ifndef WIN32
		 DebugLog (<< inet_ntop(result->h_addrtype, &tuple.ipv4.sin_addr.s_addr, str, sizeof(str)));
#endif

      }
      mCurrent = mNextHops.begin();
   }
}

bool
Resolver::isIpAddress(const Data& data)
{
   // ok, this is fairly monstrous but it works. 
   unsigned int p1,p2,p3,p4;
   int count;
   int result = sscanf( data.c_str(), 
                        "%u.%u.%u.%u%n",
                        &p1, &p2, &p3, &p4, &count );

   if ( (result == 4) && (p1 <= 255) && (p2 <= 255) && (p3 <= 255) && (p4 <= 255) && (count == data.size()) )
   {
      return true;
   }
   else
   {
      return false;
   }
}


Data
Resolver::getHostName()
{
   char buffer[256];
   if (gethostname(buffer, sizeof(buffer) < 0))
   {
      InfoLog (<< "Failed gethostname() " << strerror(errno));
      return "localhost";
   }
   else
   {
      return Data(buffer);
   }
}
