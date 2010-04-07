=begin
  Copyright(c) 2006 arton
=end

require 'rbconfig'

module RjbConf
  dir = File.join(File.dirname(File.dirname(__FILE__)), 'data')
  if File.exist?(dir)
    datadir = dir
  else
    datadir = Config::CONFIG['datadir']
  end
  BRIDGE_FILE = File.join(datadir, 'rjb', 'jp', 'co', 'infoseek', 'hp',
                          'arton', 'rjb', 'RBridge.class')
  unless File.exist?(BRIDGE_FILE)
    raise 'bridge file not found'
  end
end

require 'rjbcore'

module Rjb
  @@org_import = instance_method(:import)
  def import(s)
    o = @@org_import.bind(self).call(s)
    o.instance_eval do
      @user_initialize = nil
      @org_new ||= method(:new)
      @org_new_with_sig ||= method(:new_with_sig)
      def new_with_sig(*args)
        prepare_proxy(@org_new_with_sig(*args))
      end
      def new(*args)
        prepare_proxy(@org_new.call(*args))
      end
      def class_eval(&proc)
        @user_initialize = proc
      end
      private
      def prepare_proxy(pxy)
        pxy.instance_eval do
          def include(*mod)
            extend *mod
          end
        end  
        pxy.instance_eval &@user_initialize if @user_initialize
        pxy
      end
    end  
    o
  end
end
