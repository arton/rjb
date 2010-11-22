require 'rjb'

if Rjb::VERSION < '1.3.4'
  $stderr.puts "require rjb-1.3.4 or later, bye."
  exit 1
end

class ZipFile
  include Enumerable
  Zip = Rjb::import('java.util.zip.ZipFile')
  def initialize(file, &block)
    @zipfile = Zip.new(file)
    if block
      yield self
      @zipfile.close
    end
  end
  def close
    @zipfile.close
  end
  def each(&block)
    unless block
      Enumerator.new(self)
    else
      e = @zipfile.entries
      while e.has_more_elements
        yield e.next_element
      end
    end
  end
  def size
    @zipfile.size
  end
  def unzip(ent)
    if String === ent
      ent = @zipfile.entry(ent)
    end
    is = @zipfile.input_stream(ent)
    buff = "\0" * 4096
    File.open(ent.name, 'wb') do |fout|
      loop do
        len = is.read(buff, 0, buff.size)
        break if len < 0
        fout.write(buff[0, len])
      end
      is.close
    end
  end
end

if __FILE__ == $0
  if ARGV.size == 0
    puts 'usage: ruby unzip.rb filename'
  else
    ARGV.each do |file|
      ZipFile.new(file) do |zip|
        zip.each do |f|
          puts "#{f.name}, #{f.size}"
          unless f.directory?
            zip.unzip(f)
          end
        end
      end
    end
  end
end
