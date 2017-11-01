#include "threadpool.h"

#include <memory>
#include <iostream>
#include <string>
#include <thread>
#include <fstream>

#include <grpc++/grpc++.h>
#include <grpc/support/log.h>

#include "store.grpc.pb.h"
#include "vendor.grpc.pb.h"

#define MAX_VENDORS 16

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using store::Store;
using store::ProductReply;
using store::ProductQuery;
using store::ProductInfo;
//client things
using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using vendor::Vendor;
using vendor::BidQuery;
using vendor::BidReply;

std::string filename = "vendor_addresses.txt";
unsigned int numberOfVendors = 0;
std::vector<std::string> vendorList;

void populateVendors () {
	std::ifstream myfile (filename);
	if (myfile.is_open()) {
	  std::string ip_addr;
	  while (getline(myfile, ip_addr)) {
		vendorList.push_back(ip_addr);
		printf("added to vendor list %s\n",ip_addr.c_str());
		numberOfVendors++;
	  }
	  myfile.close();
	}
	else {
	  std::cerr << "Failed to open file " << filename << std::endl;
	}
}

class askVendors final {
	private:
		std::unique_ptr<Vendor::Stub> stub_[MAX_VENDORS];
		std::vector<std::unique_ptr<Vendor::Stub>> stubList = std::vector<std::unique_ptr<Vendor::Stub>>(MAX_VENDORS);
	public:
		askVendors(std::vector<std::string> vendorList) {
			std::vector<std::unique_ptr<Vendor::Stub>>::iterator stubit = stubList.begin();
			int i=0; //will be used to index stub array
			for(std::vector<std::string>::iterator it = vendorList.begin(); it != vendorList.end(); ++it) {
				/* std::cout << *it; ... */
				std::shared_ptr<Channel> channel = grpc::CreateChannel(*it, grpc::InsecureChannelCredentials());
				Vendor::Stub temp(channel);
				*stubit = Vendor::NewStub(channel);
				stub_[i] = Vendor::NewStub(channel);
				stubit++;
				i++;
			}
		}

		void getProductBid (const std::string& name, std::vector<BidReply>* result) {
			BidQuery request[MAX_VENDORS];
			for(int i=0; i<numberOfVendors;i++) {
				request[i].set_product_name(name);
			}
			BidReply reply[MAX_VENDORS];
			ClientContext context[MAX_VENDORS];
			CompletionQueue cq;
			Status status[MAX_VENDORS];

			std::unique_ptr<ClientAsyncResponseReader<BidReply> > rpc[MAX_VENDORS];

			for(int i=0; i<numberOfVendors; i++) {
				rpc[i] = stub_[i]->AsyncgetProductBid(&context[i],request[i],&cq);
				rpc[i]->Finish(&reply[i],&status[i],&vendorList[i]);
			}
			void* got_tag;
			bool ok = false;
			int doneCount = 0;
			while(doneCount < numberOfVendors) {
				//ok = false;
				GPR_ASSERT(cq.Next(&got_tag, &ok));
				GPR_ASSERT(ok);
				for(int i=0; i<numberOfVendors; i++) {
					if(got_tag == &vendorList[i]) {
						if(status[i].ok()) {
							result->push_back(reply[i]);
							doneCount++;
							break;
						} else {
							printf("getProductBid:: RPC failed\n");
							break;
						}						
					}
				}
				
			}
		}
};

class ServerImpl final { 
	public:
		~ServerImpl() {
		server_->Shutdown();
		// Always shutdown the completion queue after the server.
		cq_->Shutdown();
		}
	// There is no shutdown handling in this code.
	void Run() {
		std::string server_address("0.0.0.0:50001");
	
		ServerBuilder builder;
		// Listen on the given address without any authentication mechanism.
		builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
		// Register "service_" as the instance through which we'll communicate with
		// clients. In this case it corresponds to an *asynchronous* service.
		builder.RegisterService(&service_);
		// Get hold of the completion queue used for the asynchronous communication
		// with the gRPC runtime.
		cq_ = builder.AddCompletionQueue();
		// Finally assemble the server.
		server_ = builder.BuildAndStart();
		std::cout << "Server listening on " << server_address << std::endl;
	
		// Proceed to the server's main loop.
		HandleRpcs();
	  }

	private:
		// Class encompasing the state and logic needed to serve a request.
		class CallData {
		 public:
		  // Take in the "service" instance (in this case representing an asynchronous
		  // server) and the completion queue "cq" used for asynchronous communication
		  // with the gRPC runtime.
		  CallData(Store::AsyncService* service, ServerCompletionQueue* cq)
			  : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE) {
			// Invoke the serving logic right away.
			Proceed();
		  }
	  
		  void Proceed() {
			if (status_ == CREATE) {
			  // Make this instance progress to the PROCESS state.
			  status_ = PROCESS;
	  
			  // As part of the initial CREATE state, we *request* that the system
			  // start processing SayHello requests. In this request, "this" acts are
			  // the tag uniquely identifying the request (so that different CallData
			  // instances can serve different requests concurrently), in this case
			  // the memory address of this CallData instance.
			  service_->RequestgetProducts(&ctx_, &request_, &responder_, cq_, cq_,this);
			} else if (status_ == PROCESS) {
			  // Spawn a new CallData instance to serve new clients while we process
			  // the one for this CallData. The instance will deallocate itself as
			  // part of its FINISH state.
			  new CallData(service_, cq_);
	  
			  // The actual processing.
			  //std::string prefix("Hello ");
			  ProductInfo* info;
			  //info = reply_.add_products();
			  //info->set_vendor_id("test1");
			  //info->set_price(4321.0);
			  std::string name = request_.product_name();
			  askVendors* vendorInterface = new askVendors(vendorList);
			  std::vector<BidReply> result;
			  vendorInterface->getProductBid(name,&result);
			  for(std::vector<BidReply>::iterator it = result.begin(); it != result.end(); ++it) {
				info = reply_.add_products();
				info->set_vendor_id(it->vendor_id());
				info->set_price(it->price());
			  }
			  delete vendorInterface;
			  // And we are done! Let the gRPC runtime know we've finished, using the
			  // memory address of this instance as the uniquely identifying tag for
			  // the event.
			  status_ = FINISH;
			  responder_.Finish(reply_, Status::OK, this);
			} else {
			  GPR_ASSERT(status_ == FINISH);
			  // Once in the FINISH state, deallocate ourselves (CallData).
			  
			  delete this;
			}
		  }
	  
		 private:
		  // The means of communication with the gRPC runtime for an asynchronous
		  // server.
		  Store::AsyncService* service_;
		  // The producer-consumer queue where for asynchronous server notifications.
		  ServerCompletionQueue* cq_;
		  // Context for the rpc, allowing to tweak aspects of it such as the use
		  // of compression, authentication, as well as to send metadata back to the
		  // client.
		  ServerContext ctx_;
	  
		  // What we get from the client.
		  ProductQuery request_;
		  // What we send back to the client.
		  ProductReply reply_;

		
	  
		  // The means to get back to the client.
		  ServerAsyncResponseWriter<ProductReply> responder_;
	  
		  // Let's implement a tiny state machine with the following states.
		  enum CallStatus { CREATE, PROCESS, FINISH };
		  CallStatus status_;  // The current serving state.
		};
	  
		// This can be run in multiple threads if needed.
		void HandleRpcs() {
		  // Spawn a new CallData instance to serve new clients.
		  new CallData(&service_, cq_.get());
		  void* tag;  // uniquely identifies a request.
		  bool ok;
		  while (true) {
			// Block waiting to read the next event from the completion queue. The
			// event is uniquely identified by its tag, which in this case is the
			// memory address of a CallData instance.
			// The return value of Next should always be checked. This return value
			// tells us whether there is any kind of event or cq_ is shutting down.
			GPR_ASSERT(cq_->Next(&tag, &ok));
			GPR_ASSERT(ok);
			static_cast<CallData*>(tag)->Proceed();
		  }
		}
	  
		std::unique_ptr<ServerCompletionQueue> cq_;
		Store::AsyncService service_;
		std::unique_ptr<Server> server_;
};

int main(int argc, char** argv) {
	populateVendors();
	ServerImpl server;
	server.Run();
  
	return 0;
}

