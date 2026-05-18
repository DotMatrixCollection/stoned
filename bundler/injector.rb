require "bundler"

module Bundler
  class Injector
    def self.inject(contents, gems)
      additions = Array(gems).map { |gem| "gem #{gem.inspect}" }
      base = contents.to_s
      base = base.chomp
      ([base] + additions).reject { |line| line.nil? || line.empty? }.join("\n") + "\n"
    end
  end
end
