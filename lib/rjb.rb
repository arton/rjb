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

