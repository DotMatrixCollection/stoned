require "bundler"

module Bundler
  class LazySpecification
    attr_reader :name, :version, :source

    def initialize(name, version, source = nil)
      @name = name.to_s
      @version = version.to_s
      @source = source
    end

    def full_name
      "#{@name}-#{@version}"
    end

    def to_s
      full_name
    end
  end
end
