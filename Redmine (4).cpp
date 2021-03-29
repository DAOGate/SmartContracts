#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <vector>
#include <string>
using namespace eosio;

class [[eosio::contract]] Redmine:public contract  {
    public:
        using contract::contract;
        /*
        struct [[eosio::table]] hours{
            uint64_t id;
            name worker;
            float hours;
            uint64_t primary_key() const{return id;} 
            EOSLIB_SERIALIZE(hours,(id)(project)(hours));
        }
        struct [[eosio::table]] projects{
            uint64_t id;
            std::string project;
            float hours;
            eosio::asset salary;
            bool finalized;
            uint64_t primary_key() const{return id;
        
            EOSLIB_SERIALIZE(hours,(id)(project)(hours)(salary));
        }
        */
        struct [[eosio::table]] hours
        {
            name worker;
            float hours;
            uint64_t primary_key() const{return worker.value;}
        };
        struct [[eosio::table]] projects
        {
            uint64_t id;
            float hours;
            eosio::asset salary;
            bool finalized;
            uint64_t primary_key() const{return id;}
        
            //EOSLIB_SERIALIZE(hours,(id)(project)(hours)(salary));
        };
        

        struct worker{
            name account;
            float hours;
        };
        using hours_table = eosio::multi_index<name("workload"),hours>;
        using projects_table = eosio::multi_index<name("projects"),projects>;
        
        
        [[eosio::action]]
        void add(std::vector<worker> workers,const uint64_t project_id){
            check(has_auth(get_self()),"not authorized");
            //check(has_auth(name("someName")),"not authorized");
            hours_table hTable(get_self(),project_id);
            projects_table pTable(get_self(),get_self().value);
            //emplace or modify project table
            auto prj = pTable.find(project_id);
            if(prj==pTable.end()){
                pTable.emplace(get_self(),[&](auto& new_row){
                    new_row.id = project_id;
                    new_row.hours = 0;
                    new_row.salary = asset(0.0,symbol("EOS",4));
                    new_row.finalized = false;
                    
                });  
            }
            
            
            
            //emplace or modify worker table
            float workHours=0.0;
            for(auto w :workers){
                auto worker = hTable.find(w.account.value);
                if(worker==hTable.end()){
                    hTable.emplace(get_self(),[&](auto& new_row){
                        new_row.worker = w.account;
                        new_row.hours=w.hours;

                    });

                }
                else{
                    hTable.modify(worker,get_self(),[&](auto& new_row){
                        new_row.hours+=w.hours;
                    });
                }
                workHours+=w.hours;
            }
            prj = pTable.find(project_id);
            pTable.modify(prj,get_self(),[&](auto& new_row){
                    new_row.hours+=workHours;
            });
        }
        [[eosio::action]]
        void finallize(const uint64_t& project_id){
            check(has_auth(get_self()),"not authorized");
            projects_table pTable(get_self(),get_self().value);
            auto prj = pTable.find(project_id);
            pTable.modify(prj,get_self(),[&](auto& new_row){
                new_row.finalized=true;
            });
            if(prj->salary.amount!=0.0){
                distribute(project_id,prj->salary,prj->hours);
            }
                
            
            //check(has_auth(name("someName")),"not authorized");
        }
        [[eosio::action]]
        void finadd(std::vector<worker> workers,const uint64_t project_id){
            add(workers,project_id);
            finallize(project_id);


        }
        [[eosio::action]]
        void refsend(const name& referal,const name& to,const asset&  quantity,float ratio){
            symbol token = quantity.symbol;
            asset reffee = asset(quantity.amount*ratio,token);
            asset fee = asset(quantity.amount- reffee.amount,token);
            if(token==symbol("TNT",4)){
                    action(
                        permission_level{ _self, "active"_n },
                        "eosio.token"_n, "transfer"_n,
                        std::make_tuple(_self, to,fee , std::string("edex"))
                    ).send();
                    action(
                        permission_level{ _self, "active"_n },
                        "eosio.token"_n, "transfer"_n,
                        std::make_tuple(_self, referal,reffee , std::string("referal fee:")+to.to_string())
                    ).send();
                }else if(token==symbol("DGT",4)){
                    action(
                        permission_level{ _self, "active"_n },
                        "mehosimjvkkw"_n, "transfer"_n,
                        std::make_tuple(_self, to, fee, std::string("edex"))
                    ).send();
                     action(
                        permission_level{ _self, "active"_n },
                        "mehosimjvkkw"_n, "transfer"_n,
                        std::make_tuple(_self, referal, reffee, std::string("referal fee:")+to.to_string())
                    ).send();
                }

        }
        void paid(const name& from,const name& to,const asset&  quantity,const std::string& memo){
            if(to!=get_self())return;
            
            projects_table pTable(get_self(),get_self().value);
            auto prj = pTable.find(std::stoi(memo));
            if(prj!=pTable.end()){
                pTable.modify(prj,get_self(),[&](auto& new_row){
                        new_row.salary=quantity;
                });
            }
            else{
                pTable.emplace(get_self(),[&](auto& new_row){
                    new_row.id = std::stoi(memo);
                    new_row.hours = 0.0;
                    new_row.salary = quantity;
                    new_row.finalized = false;
                    
                });
            }
            if(prj->finalized!=false){
                distribute(std::stoi(memo),quantity,prj->hours);
            }
        }
        [[eosio::on_notify("eosio.token::transfer")]] 
        void paidtnt(const name& from,const name& to,const asset&  quantity,const std::string& memo){
            paid(from,to,quantity,memo);
        }

        [[eosio::on_notify("mehosimjvkkw::transfer")]]
        void paiddgt(const name& from,const name& to,const asset&  quantity,const std::string& memo){
            paid(from,to,quantity,memo);
        } 
        
        //[[eosio::action]]
        void distribute(uint64_t project,asset quantity,float hours){
            hours_table hTable(get_self(),project);
            auto itr = hTable.cbegin();
            symbol token = quantity.symbol;
            //print(token);
            for(;itr!=hTable.cend();itr++){
                asset salary = asset(quantity.amount * ((itr->hours)/hours) , quantity.symbol);
                
                if(token==symbol("TNT",4)){
                    action(
                        permission_level{ _self, "active"_n },
                        "eosio.token"_n, "transfer"_n,
                        std::make_tuple(_self, itr->worker, salary, std::string("salary"))
                    ).send();
                }else if(token==symbol("DGT",4)){
                    action(
                        permission_level{ _self, "active"_n },
                        "mehosimjvkkw"_n, "transfer"_n,
                        std::make_tuple(_self, itr->worker, salary, std::string("salary"))
                    ).send();
                }
            }

        }
};

