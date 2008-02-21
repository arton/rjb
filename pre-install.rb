=begin
 Copyright (c) 2006 arton
=end

require 'rbconfig'

if File.exist?(File.join(Config::CONFIG['sitearchdir'], 'rjb.so'))
  File.delete(File.join(Config::CONFIG['sitearchdir'], 'rjbe.so'))
end

