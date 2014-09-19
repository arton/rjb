=begin
 Copyright (c) 2006,2014 arton
=end

require 'rbconfig'

if File.exist?(File.join(RbConfig::CONFIG['sitearchdir'], 'rjb.so'))
  File.delete(File.join(RbConfig::CONFIG['sitearchdir'], 'rjb.so'))
end

