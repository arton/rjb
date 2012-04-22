=begin
Copyright(c) 2010 arton

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

$Id: rjbextension.rb 147 2010-10-23 05:10:33Z arton $

 This file is from Andreas Ronge project neo4j
   http://github.com/andreasronge/neo4j/blob/rjb/lib/rjb_ext.rb

Copyright (c) 2008 Andreas Ronge

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
=end

# Loads the JVM with the given <tt>classpath</tt> and arguments to the jre.
# All needed .jars should be included in <tt>classpath</tt>.

module Kernel
  alias rjb_original_require require

  def require(path)
    rjb_original_require(path)
  rescue LoadError
    # check that it's not a jar file
    raise unless path =~ /\.jar/

    # This will maybe use the wrong jar file from a previous version of the GEM
    # puts "LOAD PATH #{$LOAD_PATH}" 
    found_path = $LOAD_PATH.reverse.find{|p| File.exist?(File.join(p,path))}
    raise unless found_path

    abs_path = File.join(found_path, path)
    # check that the file exists
    raise unless  File.exist?(abs_path)

    # try to load it using RJB
    if Rjb::loaded?
      Rjb::add_jar abs_path
    else
      Rjb::add_classpath abs_path
    end
  end

  def load_jvm(jargs = [])
    return if Rjb::loaded?
    classpath = ENV['CLASSPATH'] ||= ''
    Rjb::load(classpath, jargs)
  end
end

class JavaPackage

  def initialize(pack_name, parent_pack = nil)
    @pack_name = pack_name
    @parent_pack = parent_pack
    @cache = {}
  end

  def method_missing(m, *args)
    # return if possible old module/class
    @cache[m] ||= create_package_or_class(m)
  end
  def create_package_or_class(m)
    method = m.to_s
    if class?(method)
      Rjb::import("#{self}.#{method}")
    else
      JavaPackage.new(method, self)
    end
  end

  def to_s
    if @parent_pack
      "#{@parent_pack.to_s}.#@pack_name"
    else
      "#@pack_name"
    end
  end

  def class?(a)
    first_letter = a[0,1]
    first_letter >= 'A' && first_letter <= 'Z'
  end

  @@cache = {}
  def self.new(pack_name, parent_pack = nil)
    @@cache[pack_name] ||= super
  end
end

module RjbConf
  # make them as singleton
  ORG = JavaPackage.new('org')
  JAVA = JavaPackage.new('java')
end

def org
  RjbConf::ORG
end

def java
  RjbConf::JAVA
end
