require "bundler"

module Bundler
  class FeatureFlag
    def initialize(settings = Bundler.settings)
      @settings = settings
    end

    def path_relative_to_cwd?
      false
    end

    def plugins?
      false
    end

    def default_install_uses_path?
      !!@settings["PATH"]
    end
  end

  def self.feature_flag
    @feature_flag ||= FeatureFlag.new
  end
end
